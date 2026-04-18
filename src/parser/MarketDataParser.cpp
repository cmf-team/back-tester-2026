#include "parser/MarketDataParser.hpp"

#include "common/TimeUtils.hpp"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <stdexcept>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cmf {

namespace {

inline uint32_t parse9(const char* p) noexcept {
  uint32_t r = 0;
  for (int i = 0; i < 9; ++i) r = r * 10u + uint32_t(p[i] - '0');
  return r;
}

// Parses an unquoted unsigned decimal. Returns cursor past the last digit.
template <class T>
inline const char* parseUInt(const char* p, T& out) noexcept {
  uint64_t v = 0;
  while (static_cast<unsigned>(*p - '0') < 10u) {
    v = v * 10u + uint64_t(*p - '0');
    ++p;
  }
  out = static_cast<T>(v);
  return p;
}

// Parses an unquoted signed decimal (ts_in_delta may be negative).
inline const char* parseInt32(const char* p, int32_t& out) noexcept {
  const bool neg = (*p == '-');
  if (neg) ++p;
  uint32_t v = 0;
  while (static_cast<unsigned>(*p - '0') < 10u) {
    v = v * 10u + uint32_t(*p - '0');
    ++p;
  }
  out = neg ? -static_cast<int32_t>(v) : static_cast<int32_t>(v);
  return p;
}

// Parses a pretty_px decimal price like "1.156100000" — quoted, 9 frac digits,
// optional leading '-'. `p` must point AT the opening '"'. Returns cursor past
// the closing '"'.
inline const char* parsePrice(const char* p, int64_t& out) noexcept {
  ++p;  // opening "
  const bool neg = (*p == '-');
  if (neg) ++p;
  int64_t ipart = 0;
  while (*p != '.') {
    ipart = ipart * 10 + int64_t(*p - '0');
    ++p;
  }
  ++p;  // '.'
  const int64_t frac = static_cast<int64_t>(parse9(p));
  p += 9;
  const int64_t px = ipart * MarketDataEvent::kPriceScale + frac;
  out = neg ? -px : px;
  return p + 1;  // past closing "
}

} // namespace

// ---------------------------------------------------------------------------
// MappedFile

MappedFile::MappedFile(const std::filesystem::path& path) {
  fd_ = ::open(path.c_str(), O_RDONLY);
  if (fd_ < 0) {
    throw std::runtime_error("open failed: " + path.string()
                             + ": " + std::strerror(errno));
  }

  struct stat st{};
  if (::fstat(fd_, &st) != 0) {
    const int err = errno;
    ::close(fd_);
    fd_ = -1;
    throw std::runtime_error(std::string("fstat failed: ") + std::strerror(err));
  }
  size_ = static_cast<std::size_t>(st.st_size);
  if (size_ == 0) return;

  int flags = MAP_PRIVATE;
#ifdef MAP_POPULATE
  flags |= MAP_POPULATE;  // Linux: pre-fault pages
#endif
  void* p = ::mmap(nullptr, size_, PROT_READ, flags, fd_, 0);
  if (p == MAP_FAILED) {
    const int err = errno;
    ::close(fd_);
    fd_ = -1;
    throw std::runtime_error(std::string("mmap failed: ") + std::strerror(err));
  }
  data_ = static_cast<const char*>(p);
  ::madvise(const_cast<void*>(static_cast<const void*>(data_)), size_,
            MADV_SEQUENTIAL);
}

MappedFile::~MappedFile() {
  if (data_) ::munmap(const_cast<char*>(data_), size_);
  if (fd_ >= 0) ::close(fd_);
}

MappedFile::MappedFile(MappedFile&& other) noexcept
    : data_(other.data_), size_(other.size_), fd_(other.fd_) {
  other.data_ = nullptr;
  other.size_ = 0;
  other.fd_   = -1;
}

MappedFile& MappedFile::operator=(MappedFile&& other) noexcept {
  if (this != &other) {
    if (data_) ::munmap(const_cast<char*>(data_), size_);
    if (fd_ >= 0) ::close(fd_);
    data_       = other.data_;
    size_       = other.size_;
    fd_         = other.fd_;
    other.data_ = nullptr;
    other.size_ = 0;
    other.fd_   = -1;
  }
  return *this;
}

// ---------------------------------------------------------------------------
// MarketDataParser

MarketDataParser::MarketDataParser(const std::filesystem::path& path)
    : file_(std::make_unique<MappedFile>(path)) {
  begin_ = file_->data();
  cur_   = begin_;
  end_   = begin_ + file_->size();
}

MarketDataParser::MarketDataParser(const char* begin, const char* end) noexcept
    : begin_(begin), cur_(begin), end_(end) {}

std::size_t MarketDataParser::bytesConsumed() const noexcept {
  return static_cast<std::size_t>(cur_ - begin_);
}

std::size_t MarketDataParser::totalBytes() const noexcept {
  return static_cast<std::size_t>(end_ - begin_);
}

bool MarketDataParser::next(MarketDataEvent& out) {
  if (cur_ >= end_) return false;
  const char* p = cur_;

  // {"ts_recv":"
  p += 12;
  out.ts_recv = parseIsoTs(p);
  p += 30;

  // ","hd":{"ts_event":"
  p += 20;
  out.ts_event = parseIsoTs(p);
  p += 30;

  // ","rtype":
  p += 10;
  p = parseUInt(p, out.rtype);

  // ,"publisher_id":
  p += 16;
  p = parseUInt(p, out.publisher_id);

  // ,"instrument_id":
  p += 17;
  p = parseUInt(p, out.instrument_id);

  // },"action":"
  p += 12;
  out.action = static_cast<Action>(*p);
  ++p;

  // ","side":"
  p += 10;
  out.side = static_cast<MdSide>(*p);
  ++p;

  // ","price":
  p += 10;
  if (*p == 'n') {                 // null
    out.price = MarketDataEvent::kUndefPrice;
    p += 4;
  } else {
    p = parsePrice(p, out.price);
  }

  // ,"size":
  p += 8;
  p = parseUInt(p, out.size);

  // ,"channel_id":
  p += 14;
  p = parseUInt(p, out.channel_id);

  // ,"order_id":"
  p += 13;
  p = parseUInt(p, out.order_id);
  // p now points AT the closing '"' - the next prefix starts with it.

  // ","flags":
  p += 10;
  p = parseUInt(p, out.flags);

  // ,"ts_in_delta":
  p += 15;
  p = parseInt32(p, out.ts_in_delta);

  // ,"sequence":
  p += 12;
  p = parseUInt(p, out.sequence);

  // ,"symbol":"
  p += 11;
  const char* symBegin = p;
  while (*p != '"') ++p;           // symbols have spaces but no quotes
  out.symbol.assign(symBegin, static_cast<std::size_t>(p - symBegin));

  // closing " + } + optional \n
  p += 2;
  if (p < end_ && *p == '\n') ++p;

  cur_ = p;
  return true;
}

} // namespace cmf
