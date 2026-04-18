#include "ingest/NdjsonReader.hpp"

#include "ingest/MboDecoder.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace cmf {

IngestStats parseNdjsonFile(const std::filesystem::path &path,
                            const MarketDataEventConsumer &consumer) {
  std::ifstream fs(path);
  if (!fs) {
    throw std::runtime_error("parseNdjsonFile: cannot open " + path.string());
  }

  IngestStats stats{};
  MboDecoder decoder;
  std::string line;
  line.reserve(4096);

  while (std::getline(fs, line)) {
    if (line.empty()) {
      continue;
    }
    const auto result = decoder.decodeLine(line);
    switch (result.outcome) {
    case DecodeOutcome::Ok: {
      const auto &e = result.event;
      if (stats.consumed == 0) {
        stats.first_ts_recv = e.ts_recv;
      } else if (e.ts_recv < stats.last_ts_recv) {
        ++stats.out_of_order_ts_recv;
      }
      stats.last_ts_recv = e.ts_recv;
      ++stats.consumed;
      consumer(e);
      break;
    }
    case DecodeOutcome::SkippedRtype:
      ++stats.skipped_rtype;
      break;
    case DecodeOutcome::ParseError:
      ++stats.skipped_parse;
      break;
    }
  }

  return stats;
}

} // namespace cmf
