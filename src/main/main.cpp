// main function for the back-tester app
// please, keep it minimalistic

#include "common/MarketDataEvent.hpp"
#include "ingest/NdjsonReader.hpp"

#include <array>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <iostream>
#include <vector>

using namespace cmf;

namespace {

constexpr std::size_t kHeadPreviewSize = 10;
constexpr std::size_t kTailPreviewSize = 10;

struct PreviewState {
  std::vector<MarketDataEvent> head;
  std::array<MarketDataEvent, kTailPreviewSize> tail{};
  std::size_t tailCount = 0;
  std::size_t tailStart = 0;
};

PreviewState gPreview;

} // namespace

namespace cmf {

// Spec-named consumer seam. Called for every decoded event; prints the first
// kHeadPreviewSize events directly, and keeps the latest kTailPreviewSize in a
// ring buffer for main() to flush after the stream ends.
void processMarketDataEvent(const MarketDataEvent &event) {
  if (gPreview.head.size() < kHeadPreviewSize) {
    gPreview.head.push_back(event);
    std::cout << event << '\n';
  }

  if (gPreview.tailCount < kTailPreviewSize) {
    gPreview.tail[gPreview.tailCount] = event;
    ++gPreview.tailCount;
  } else {
    gPreview.tail[gPreview.tailStart] = event;
    gPreview.tailStart = (gPreview.tailStart + 1) % kTailPreviewSize;
  }
}

} // namespace cmf

int main(int argc, const char *argv[]) {
  try {
    if (argc != 2) {
      std::cerr << "usage: back-tester <path-to-ndjson>\n";
      return 2;
    }
    const std::filesystem::path path{argv[1]};

    const IngestStats stats = parseNdjsonFile(path, &processMarketDataEvent);

    const std::size_t tailToPrint =
        stats.consumed > kHeadPreviewSize
            ? std::min(kTailPreviewSize, stats.consumed - kHeadPreviewSize)
            : 0;
    if (tailToPrint > 0) {
      std::cout << "\n--- tail ---\n";
      const std::size_t tailSkip = gPreview.tailCount - tailToPrint;
      for (std::size_t i = 0; i < tailToPrint; ++i) {
        const std::size_t idx =
            (gPreview.tailStart + tailSkip + i) % kTailPreviewSize;
        std::cout << gPreview.tail[idx] << '\n';
      }
    }

    std::cout << "\n"
              << "total=" << stats.consumed
              << " skipped_rtype=" << stats.skipped_rtype
              << " skipped_parse=" << stats.skipped_parse
              << " first_ts_recv=" << stats.first_ts_recv
              << " last_ts_recv=" << stats.last_ts_recv
              << " out_of_order_ts_recv=" << stats.out_of_order_ts_recv << '\n';
  } catch (std::exception &ex) {
    std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
    return 1;
  }

  return 0;
}
