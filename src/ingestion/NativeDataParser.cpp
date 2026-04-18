#include "NativeDataParser.hpp"

#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string_view>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace cmf {

class MmapGuard {
  void *mapped_;
  std::size_t size_;

public:
  MmapGuard(void *mapped, std::size_t size) : mapped_(mapped), size_(size) {}
  ~MmapGuard() {
    if (mapped_ != MAP_FAILED && mapped_ != nullptr)
      munmap(mapped_, size_);
  }
};

// Advance p past a JSON value of any type.
static void ndp_skipVal(const char *&p, const char *end) noexcept {
  while (p < end && *p == ' ')
    ++p;
  if (p >= end)
    return;
  if (*p == '"') {
    ++p;
    while (p < end && *p != '"')
      ++p;
    if (p < end)
      ++p;
  } else if (*p == '{' || *p == '[') {
    int depth = 0;
    while (p < end) {
      const char c = *p++;
      if (c == '{' || c == '[')
        ++depth;
      else if (c == '}' || c == ']') {
        if (--depth == 0)
          return;
      } else if (c == '"') {
        while (p < end && *p != '"')
          ++p;
        if (p < end)
          ++p;
      }
    }
  } else {
    while (p < end && *p != ',' && *p != '}' && *p != ']')
      ++p;
  }
}

// Inline integer parsers: faster than std::from_chars for tight loops.
static inline uint32_t parse_u32(const char *&p, const char *end) noexcept {
  uint32_t v = 0;
  while (p < end && (unsigned char)(*p - '0') <= 9u)
    v = v * 10 + (*p++ - '0');
  return v;
}

static inline uint64_t parse_u64(const char *&p, const char *end) noexcept {
  uint64_t v = 0;
  while (p < end && (unsigned char)(*p - '0') <= 9u)
    v = v * 10 + (*p++ - '0');
  return v;
}

static int32_t parse_i32(const char *&p, const char *end) noexcept {
  bool neg = (p < end && *p == '-') ? (++p, true) : false;
  uint32_t v = parse_u32(p, end);
  return neg ? -static_cast<int32_t>(v) : static_cast<int32_t>(v);
}

// Branchless fixed 9-digit nano parse — no loop, no branch, no multiply chain
// Assumes well-formed data (valid for backtesting — data is pre-validated)
static int64_t parse_nanos_fixed9(const char *p) noexcept {
  return static_cast<int64_t>(p[0] - '0') * 100'000'000LL +
         static_cast<int64_t>(p[1] - '0') * 10'000'000LL +
         static_cast<int64_t>(p[2] - '0') * 1'000'000LL +
         static_cast<int64_t>(p[3] - '0') * 100'000LL +
         static_cast<int64_t>(p[4] - '0') * 10'000LL +
         static_cast<int64_t>(p[5] - '0') * 1'000LL +
         static_cast<int64_t>(p[6] - '0') * 100LL +
         static_cast<int64_t>(p[7] - '0') * 10LL +
         static_cast<int64_t>(p[8] - '0');
}

// Fast fixed-format ISO 8601 parser: "2026-04-01T00:00:21.480470598Z"
// Replaces strptime+timegm with pure integer arithmetic (Howard Hinnant's
// algorithm).
static NanoTime ndp_parseIso8601Nanos(std::string_view s) {
  static constexpr int64_t POW10[10] = {
      1'000'000'000LL, 100'000'000LL, 10'000'000LL, 1'000'000LL, 100'000LL,
      10'000LL,        1'000LL,       100LL,        10LL,        1LL};
  if (s.size() < 19) {
    char ebuf[64];
    std::size_t elen = std::min(s.size(), sizeof(ebuf) - 1);
    std::memcpy(ebuf, s.data(), elen);
    ebuf[elen] = '\0';
    throw std::runtime_error(std::string("bad timestamp: ") + ebuf);
  }
  const char *p = s.data();
  int y = (p[0] - '0') * 1000 + (p[1] - '0') * 100 + (p[2] - '0') * 10 +
          (p[3] - '0');
  int mo = (p[5] - '0') * 10 + (p[6] - '0');
  int dy = (p[8] - '0') * 10 + (p[9] - '0');
  int H = (p[11] - '0') * 10 + (p[12] - '0');
  int M = (p[14] - '0') * 10 + (p[15] - '0');
  int S = (p[17] - '0') * 10 + (p[18] - '0');

  // Civil-to-Unix-days (Hinnant):
  // https://howardhinnant.github.io/date_algorithms.html
  int z = y - (mo <= 2 ? 1 : 0);
  int era = (z >= 0 ? z : z - 399) / 400;
  int yoe = z - era * 400;
  int doy = (153 * (mo + (mo <= 2 ? 9 : -3)) + 2) / 5 + dy - 1;
  int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  int64_t days = static_cast<int64_t>(era) * 146097 + doe - 719468;

  int64_t sec = days * 86400LL + H * 3600LL + M * 60LL + S;

  int64_t nanos = 0;
  if (s.size() >= 29 && s[19] == '.') [[likely]]
    nanos = parse_nanos_fixed9(s.data() + 20);
  else if (s.size() > 19 && s[19] == '.') {
    // fallback for truncated sub-second precision (malformed data)
    int digits = 0;
    std::size_t i = 20;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9' && digits < 9) {
      nanos = nanos * 10 + (s[i] - '0');
      ++digits;
      ++i;
    }
    nanos *= POW10[digits];
  }
  return sec * 1'000'000'000LL + nanos;
}

// Single forward scan of the nested "hd" object.
// Keys: rtype(5), ts_event(8), publisher_id(12), instrument_id(13)
static void ndp_parseHd(const char *&p, const char *end, MarketDataEvent &e) {
  if (p >= end || *p != '{')
    return;
  ++p;
  while (p < end) {
    while (p < end && (*p == ' ' || *p == ','))
      ++p;
    if (p >= end || *p == '}') {
      if (p < end && *p == '}')
        ++p;
      break;
    }
    if (*p != '"') {
      ++p;
      continue;
    }
    ++p;
    const char *ks = p;
    while (p < end && *p != '"')
      ++p;
    const std::size_t klen = static_cast<std::size_t>(p - ks);
    if (p < end)
      ++p;
    while (p < end && (*p == ':' || *p == ' '))
      ++p;

    switch (klen) {
    case 5: // "rtype"
      if (p < end && *p >= '0' && *p <= '9') {
        e.rtype = static_cast<RType>(parse_u32(p, end));
      } else
        ndp_skipVal(p, end);
      break;
    case 8: // "ts_event"
      if (p < end && *p == '"') {
        ++p;
        const char *vs = p;
        while (p < end && *p != '"')
          ++p;
        if (p > vs)
          e.ts_event =
              ndp_parseIso8601Nanos({vs, static_cast<std::size_t>(p - vs)});
        if (p < end)
          ++p;
      } else
        ndp_skipVal(p, end);
      break;
    case 12: // "publisher_id"
      if (p < end && *p >= '0' && *p <= '9')
        e.publisher_id = static_cast<uint16_t>(parse_u32(p, end));
      else
        ndp_skipVal(p, end);
      break;
    case 13: // "instrument_id"
      if (p < end && *p >= '0' && *p <= '9')
        e.instrument_id = parse_u32(p, end);
      else
        ndp_skipVal(p, end);
      break;
    default:
      ndp_skipVal(p, end);
      break;
    }
  }
}

// Single-pass parser: scans the outer JSON object once.
// Key dispatch: first char + key length (no conflicts for this schema).
// Outer keys: hd(2), side(4), size(4), price(5), flags(5), action(6),
//   symbol(6), ts_recv(7), order_id(8), sequence(8), channel_id(10),
//   ts_in_delta(11)
static MarketDataEvent ndp_parseLine(const char *&p, const char *end) {
  MarketDataEvent e;

  if (p >= end || *p != '{')
    throw std::runtime_error("missing ts_recv");
  ++p;

  while (p < end) {
    while (p < end && (*p == ' ' || *p == ','))
      ++p;
    if (p >= end || *p == '}')
      break;
    if (*p != '"') {
      ++p;
      continue;
    }
    ++p;
    const char *ks = p;
    while (p < end && *p != '"')
      ++p;
    const std::size_t klen = static_cast<std::size_t>(p - ks);
    if (p < end)
      ++p;
    while (p < end && (*p == ':' || *p == ' '))
      ++p;

    switch (klen) {
    case 2: // "hd"
      if (ks[0] == 'h')
        ndp_parseHd(p, end, e);
      else
        ndp_skipVal(p, end);
      break;
    case 4: // "side" (ks[2]=='d') or "size" (ks[2]=='z')
      if (ks[0] == 's') {
        if (ks[2] == 'd') { // side
          if (p < end && *p == '"') {
            ++p;
            if (p < end)
              e.side = static_cast<Side>(*p++);
            while (p < end && *p != '"')
              ++p;
            if (p < end)
              ++p;
          } else
            ndp_skipVal(p, end);
        } else { // size
          if (p < end && *p >= '0' && *p <= '9')
            e.size = parse_u32(p, end);
          else
            ndp_skipVal(p, end);
        }
      } else
        ndp_skipVal(p, end);
      break;
    case 5: // "price" (ks[0]=='p') or "flags" (ks[0]=='f')
      if (ks[0] == 'p') {
        if (p < end && *p == 'n') {
          p += 4; // "null"
        } else if (p < end && *p == '"') {
          ++p;
          const char *vs = p;
          while (p < end && *p != '"')
            ++p;
          if (p > vs) {
            const char *pp = vs;
            bool neg = (pp < p && *pp == '-') ? (++pp, true) : false;
            int64_t int_part = 0;
            while (pp < p && *pp != '.')
              int_part = int_part * 10 + (*pp++ - '0');
            int64_t frac_part = 0;
            if (pp < p && *pp == '.') {
              ++pp;
              int digits = 0;
              while (pp < p && digits < 9) {
                frac_part = frac_part * 10 + (*pp++ - '0');
                ++digits;
              }
              while (digits++ < 9)
                frac_part *= 10;
            }
            int64_t raw = int_part * 1'000'000'000LL + frac_part;
            e.price = neg ? -raw : raw;
          }
          if (p < end)
            ++p;
        } else
          ndp_skipVal(p, end);
      } else if (ks[0] == 'f') {
        if (p < end && *p >= '0' && *p <= '9') {
          e.flags = static_cast<Flags>(parse_u32(p, end));
        } else
          ndp_skipVal(p, end);
      } else
        ndp_skipVal(p, end);
      break;
    case 6: // "action" (ks[0]=='a') or "symbol" (ks[0]=='s')
      if (ks[0] == 'a') {
        if (p < end && *p == '"') {
          ++p;
          if (p < end)
            e.action = static_cast<Action>(*p++);
          while (p < end && *p != '"')
            ++p;
          if (p < end)
            ++p;
        } else
          ndp_skipVal(p, end);
      } else
        ndp_skipVal(p, end);
      break;
    case 7: // "ts_recv"
      if (ks[0] == 't' && p < end && *p == '"') {
        ++p;
        const char *vs = p;
        while (p < end && *p != '"')
          ++p;
        if (p > vs)
          e.ts_recv =
              ndp_parseIso8601Nanos({vs, static_cast<std::size_t>(p - vs)});
        if (p < end)
          ++p;
      } else
        ndp_skipVal(p, end);
      break;
    case 8: // "order_id" (ks[0]=='o') or "sequence" (ks[0]=='s')
      if (ks[0] == 'o') {
        if (p < end && *p == '"') {
          ++p;
          e.order_id = parse_u64(p, end);
          while (p < end && *p != '"')
            ++p;
          if (p < end)
            ++p;
        } else
          ndp_skipVal(p, end);
      } else if (ks[0] == 's') {
        if (p < end && *p >= '0' && *p <= '9')
          e.sequence = parse_u32(p, end);
        else
          ndp_skipVal(p, end);
      } else
        ndp_skipVal(p, end);
      break;
    case 10: // "channel_id"
      if (ks[0] == 'c' && p < end && *p >= '0' && *p <= '9')
        e.channel_id = static_cast<uint16_t>(parse_u32(p, end));
      else
        ndp_skipVal(p, end);
      break;
    case 11: // "ts_in_delta"
      if (ks[0] == 't' && p < end && (*p == '-' || (*p >= '0' && *p <= '9')))
        e.ts_in_delta = parse_i32(p, end);
      else
        ndp_skipVal(p, end);
      break;
    default:
      ndp_skipVal(p, end);
      break;
    }
  }

  if (p < end && *p == '}')
    ++p;

  if (e.ts_recv == 0)
    throw std::runtime_error("missing ts_recv");
  return e;
}

void NativeDataParser::parse_inner(
    const std::function<void(const MarketDataEvent &)> &f) const {
  int fd = ::open(path_.c_str(), O_RDONLY);
  if (fd == -1)
    throw std::runtime_error("NativeDataParser: cannot open " + path_.string());

  struct stat st{};
  if (fstat(fd, &st) == -1) {
    ::close(fd);
    throw std::runtime_error("NativeDataParser: fstat failed for " +
                             path_.string());
  }

  if (st.st_size == 0) {
    ::close(fd);
    return;
  }

  void *mapped = mmap(nullptr, static_cast<std::size_t>(st.st_size), PROT_READ,
                      MAP_PRIVATE, fd, 0);
  ::close(fd);

  if (mapped == MAP_FAILED)
    throw std::runtime_error("NativeDataParser: mmap failed for " +
                             path_.string());

  MmapGuard guard(mapped, st.st_size);
  madvise(mapped, st.st_size, MADV_SEQUENTIAL);

  const char *data = static_cast<const char *>(mapped);
  const char *end = data + st.st_size;
  const char *pos = data;

  while (pos < end) {
    while (pos < end && (*pos == '\n' || *pos == '\r' || *pos == ' '))
      ++pos;
    if (pos >= end)
      break;
    try {
      f(ndp_parseLine(pos, end));
    } catch (const std::exception &ex) {
      std::fprintf(stderr, "NativeDataParser: skipping line: %s\n", ex.what());
      const char *nl = static_cast<const char *>(
          std::memchr(pos, '\n', static_cast<std::size_t>(end - pos)));
      pos = nl ? nl + 1 : end;
    }
  }
}

} // namespace cmf
