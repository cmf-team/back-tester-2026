#include "pipeline/Producer.hpp"

#include <utility>

namespace cmf {

Producer::Producer(std::filesystem::path path,
                   std::shared_ptr<EventQueue<MarketDataEvent>> out)
    : path_(std::move(path)), out_(std::move(out)) {}

Producer::~Producer() {
  // Defensive: in case someone forgets to join() us before destruction.
  if (worker_.joinable())
    worker_.join();
}

void Producer::start() {
  worker_ = std::thread([this] { run(); });
}

void Producer::join() {
  if (worker_.joinable())
    worker_.join();
}

void Producer::run() {
  try {
    FileMarketDataSource src(path_);
    MarketDataEvent ev;
    while (src.next(ev)) {
      // push() blocks under back-pressure; this is what we want -- it
      // throttles the producer to the consumer's rate.
      if (!out_->push(std::move(ev)))
        break; // queue closed externally
      ++produced_;
    }
    skipped_   = src.skippedCount();
    malformed_ = src.malformedCount();
  } catch (...) {
    error_ = std::current_exception();
  }
  finished_ = true;
  // Signal end-of-stream to whoever drains the queue.
  out_->close();
}

} // namespace cmf
