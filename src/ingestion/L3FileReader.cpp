#include "ingestion/L3FileReader.hpp"

#include <zstd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace cmf {

namespace {

// Convert a single raw action byte into the corresponding Action enum.
// Unknown codes are mapped to Action::None (the caller can still see the
// original byte via ev.flags/diagnostics if needed).
Action decodeAction(char c) noexcept {
  switch (c) {
  case 'A':
    return Action::Add;
  case 'C':
    return Action::Cancel;
  case 'M':
    return Action::Modify;
  case 'T':
    return Action::Trade;
  case 'F':
    return Action::Fill;
  case 'R':
    return Action::Clear;
  default:
    return Action::None;
  }
}

// Convert a raw side byte ('B' / 'A' / 'N' / 0) into a Side enum.
// Note: 'A' in EOBI/MBO means "ask" (sell side resting order).
Side decodeSide(char c) noexcept {
  switch (c) {
  case 'B':
    return Side::Buy;
  case 'A':
    return Side::Sell;
  default:
    return Side::None;
  }
}

// Little-endian unaligned load.
template <typename T> T loadLE(const std::uint8_t *src) noexcept {
  T value{};
  std::memcpy(&value, src, sizeof(T));
  return value;
}

// Owning wrapper around a libc FILE*.
class FileHandle {
public:
  explicit FileHandle(const std::string &path)
      : fp_(std::fopen(path.c_str(), "rb")) {
    if (fp_ == nullptr) {
      throw std::runtime_error("failed to open '" + path +
                               "': " + std::strerror(errno));
    }
  }
  FileHandle(const FileHandle &) = delete;
  FileHandle &operator=(const FileHandle &) = delete;
  ~FileHandle() {
    if (fp_ != nullptr) {
      std::fclose(fp_);
    }
  }
  [[nodiscard]] std::FILE *get() const noexcept { return fp_; }

private:
  std::FILE *fp_{nullptr};
};

// Owning wrapper around a ZSTD_DCtx*.
class ZstdDCtx {
public:
  ZstdDCtx() : ctx_(ZSTD_createDCtx()) {
    if (ctx_ == nullptr) {
      throw std::runtime_error("failed to create ZSTD_DCtx");
    }
  }
  ZstdDCtx(const ZstdDCtx &) = delete;
  ZstdDCtx &operator=(const ZstdDCtx &) = delete;
  ~ZstdDCtx() { ZSTD_freeDCtx(ctx_); }
  [[nodiscard]] ZSTD_DCtx *get() const noexcept { return ctx_; }

private:
  ZSTD_DCtx *ctx_{nullptr};
};

} // namespace

void decodeRecord(const std::uint8_t *raw, MarketDataEvent &out) noexcept {
  using L = L3RecordLayout;

  out.ts_event = loadLE<std::int64_t>(raw + L::ts_event);
  out.ts_recv = loadLE<std::int64_t>(raw + L::ts_recv);

  out.action = decodeAction(static_cast<char>(raw[L::action]));
  out.side = decodeSide(static_cast<char>(raw[L::side]));

  out.price = loadLE<double>(raw + L::price);
  out.size = loadLE<std::int64_t>(raw + L::size);

  out.channel_id = loadLE<std::int32_t>(raw + L::channel_id);
  out.order_id = loadLE<std::uint64_t>(raw + L::order_id);
  out.flags = raw[L::flags];
  out.ts_in_delta = loadLE<std::int32_t>(raw + L::ts_in_delta);
  out.sequence = loadLE<std::int32_t>(raw + L::sequence);

  std::memcpy(out.symbol.data(), raw + L::symbol, kMaxSymbolLen);

  out.rtype = loadLE<std::int32_t>(raw + L::rtype);
  out.publisher_id = loadLE<std::uint32_t>(raw + L::publisher_id);
  out.instrument_id = loadLE<std::int32_t>(raw + L::instrument_id);
}

std::size_t readL3ZstFile(const std::string &path,
                          const MarketDataEventHandler &onEvent) {
  FileHandle file(path);
  ZstdDCtx dctx;

  const std::size_t inCap = ZSTD_DStreamInSize();
  const std::size_t outCap = ZSTD_DStreamOutSize();
  std::vector<std::uint8_t> inBuf(inCap);
  std::vector<std::uint8_t> outBuf(outCap);

  // Leftover bytes from the previous iteration that didn't fill a full record.
  std::vector<std::uint8_t> carry;
  carry.reserve(kRecordSize);

  MarketDataEvent event;
  std::size_t processed = 0;
  std::size_t lastRet = 0;
  bool eof = false;

  while (!eof) {
    const std::size_t nread =
        std::fread(inBuf.data(), 1, inBuf.size(), file.get());
    if (nread == 0) {
      if (std::ferror(file.get()) != 0) {
        throw std::runtime_error("read error on '" + path +
                                 "': " + std::strerror(errno));
      }
      eof = true;
      if (lastRet != 0) {
        throw std::runtime_error("unexpected EOF in zstd stream: '" + path +
                                 "'");
      }
      break;
    }

    ZSTD_inBuffer in{inBuf.data(), nread, 0};
    while (in.pos < in.size) {
      ZSTD_outBuffer outb{outBuf.data(), outBuf.size(), 0};
      const std::size_t ret =
          ZSTD_decompressStream(dctx.get(), &outb, &in);
      if (ZSTD_isError(ret) != 0) {
        throw std::runtime_error(
            std::string{"ZSTD_decompressStream failed: "} +
            ZSTD_getErrorName(ret));
      }
      lastRet = ret;

      // Feed decompressed bytes into fixed-size record boundaries.
      const std::uint8_t *p = outBuf.data();
      std::size_t remaining = outb.pos;

      if (!carry.empty()) {
        const std::size_t need = kRecordSize - carry.size();
        const std::size_t take = std::min(need, remaining);
        carry.insert(carry.end(), p, p + take);
        p += take;
        remaining -= take;
        if (carry.size() == kRecordSize) {
          decodeRecord(carry.data(), event);
          onEvent(event);
          ++processed;
          carry.clear();
        }
      }

      while (remaining >= kRecordSize) {
        decodeRecord(p, event);
        onEvent(event);
        ++processed;
        p += kRecordSize;
        remaining -= kRecordSize;
      }

      if (remaining > 0) {
        carry.assign(p, p + remaining);
      }
    }
  }

  if (!carry.empty()) {
    throw std::runtime_error("trailing " + std::to_string(carry.size()) +
                             " bytes do not form a complete record in '" +
                             path + "'");
  }

  return processed;
}

} // namespace cmf
