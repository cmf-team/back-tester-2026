#include "ingest/FolderIngest.hpp"

#include "ingest/BlockingQueue.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <compare>
#include <cstdint>
#include <exception>
#include <memory>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace cmf {

namespace {

using Clock = std::chrono::steady_clock;

struct OrderedEvent {
  MarketDataEvent event;
  std::uint32_t source_index = 0;
};

struct EventBlock {
  std::vector<OrderedEvent> events;
};

using BlockQueue = BlockingQueue<EventBlock>;
using BlockQueuePtr = std::shared_ptr<BlockQueue>;

struct ProducerResult {
  IngestStats stats{};
};

struct DispatchResult {
  std::size_t total = 0;
  std::size_t out_of_order_ts_recv = 0;
  std::uint64_t first_ts_recv = UNDEF_TIMESTAMP;
  std::uint64_t last_ts_recv = UNDEF_TIMESTAMP;
};

std::strong_ordering compareOrderedEvent(const OrderedEvent &lhs,
                                         const OrderedEvent &rhs) noexcept {
  if (const auto by_event = mdeOrderKey(lhs.event) <=> mdeOrderKey(rhs.event);
      by_event != 0) {
    return by_event;
  }
  return lhs.source_index <=> rhs.source_index;
}

bool orderedEventLess(const OrderedEvent &lhs,
                      const OrderedEvent &rhs) noexcept {
  return compareOrderedEvent(lhs, rhs) < 0;
}

class RunControl {
public:
  void registerQueue(const BlockQueuePtr &queue) {
    std::lock_guard lock{queues_mutex_};
    queues_.push_back(queue);
  }

  void captureCurrentException() {
    {
      std::lock_guard lock{error_mutex_};
      if (!error_) {
        error_ = std::current_exception();
      }
    }
    cancelled_.store(true);
    closeAllQueues();
  }

  void closeAllQueues() {
    std::vector<BlockQueuePtr> snapshot;
    {
      std::lock_guard lock{queues_mutex_};
      snapshot = queues_;
    }
    for (const auto &queue : snapshot) {
      if (queue) {
        queue->close();
      }
    }
  }

  bool cancelled() const noexcept { return cancelled_.load(); }

  void rethrowIfNeeded() const {
    std::lock_guard lock{error_mutex_};
    if (error_) {
      std::rethrow_exception(error_);
    }
  }

private:
  mutable std::mutex error_mutex_;
  std::mutex queues_mutex_;
  std::exception_ptr error_;
  std::atomic<bool> cancelled_{false};
  std::vector<BlockQueuePtr> queues_;
};

BlockQueuePtr makeQueue(std::size_t capacity, RunControl &control) {
  auto queue = std::make_shared<BlockQueue>(capacity);
  control.registerQueue(queue);
  return queue;
}

EventBlock makeBlock(std::size_t batch_size) {
  EventBlock block;
  block.events.reserve(batch_size);
  return block;
}

bool flushBlock(const BlockQueuePtr &queue, EventBlock &block,
                std::size_t batch_size) {
  if (block.events.empty()) {
    return true;
  }
  if (!queue->push(std::move(block))) {
    return false;
  }
  block = makeBlock(batch_size);
  return true;
}

class StreamCursor {
public:
  explicit StreamCursor(BlockQueuePtr queue) : queue_{std::move(queue)} {}

  bool ensureCurrent() {
    while (offset_ >= current_.events.size()) {
      current_ = EventBlock{};
      offset_ = 0;
      if (!queue_->pop(current_)) {
        return false;
      }
    }
    return true;
  }

  // Precondition: ensureCurrent() was called and returned true.
  const OrderedEvent &current() const noexcept {
    assert(offset_ < current_.events.size());
    return current_.events[offset_];
  }

  void advance() noexcept { ++offset_; }

private:
  BlockQueuePtr queue_;
  EventBlock current_;
  std::size_t offset_ = 0;
};

void producerThread(const std::filesystem::path &path, std::uint32_t source_index,
                    const BlockQueuePtr &output, ProducerResult &result,
                    std::size_t batch_size, RunControl &control) {
  try {
    EventBlock block = makeBlock(batch_size);
    result.stats = parseNdjsonFile(
        path, MarketDataEventVisitor{[&](const MarketDataEvent &event) {
          if (control.cancelled()) {
            return false;
          }
          block.events.push_back(OrderedEvent{event, source_index});
          if (block.events.size() < batch_size) {
            return true;
          }
          return flushBlock(output, block, batch_size);
        }});
    if (!control.cancelled()) {
      flushBlock(output, block, batch_size);
    }
  } catch (...) {
    control.captureCurrentException();
  }
  output->close();
}

void mergeTwoQueues(const BlockQueuePtr &left, const BlockQueuePtr &right,
                    const BlockQueuePtr &output, std::size_t batch_size,
                    RunControl &control) {
  try {
    StreamCursor left_cursor{left};
    StreamCursor right_cursor{right};
    bool has_left = left_cursor.ensureCurrent();
    bool has_right = right_cursor.ensureCurrent();
    EventBlock output_block = makeBlock(batch_size);

    while (!control.cancelled() && (has_left || has_right)) {
      if (!has_right ||
          (has_left &&
           !orderedEventLess(right_cursor.current(), left_cursor.current()))) {
        output_block.events.push_back(left_cursor.current());
        left_cursor.advance();
        has_left = left_cursor.ensureCurrent();
      } else {
        output_block.events.push_back(right_cursor.current());
        right_cursor.advance();
        has_right = right_cursor.ensureCurrent();
      }

      if (output_block.events.size() == batch_size &&
          !flushBlock(output, output_block, batch_size)) {
        break;
      }
    }

    if (!control.cancelled()) {
      flushBlock(output, output_block, batch_size);
    }
  } catch (...) {
    control.captureCurrentException();
  }
  output->close();
}

void flatMergerThread(const std::vector<BlockQueuePtr> &inputs,
                      const BlockQueuePtr &output, std::size_t batch_size,
                      RunControl &control) {
  struct HeapEntry {
    OrderedEvent ordered_event;
    std::size_t stream_index = 0;
  };

  struct HeapGreater {
    bool operator()(const HeapEntry &lhs, const HeapEntry &rhs) const noexcept {
      return orderedEventLess(rhs.ordered_event, lhs.ordered_event);
    }
  };

  try {
    std::vector<StreamCursor> cursors;
    cursors.reserve(inputs.size());
    std::priority_queue<HeapEntry, std::vector<HeapEntry>, HeapGreater> heap;

    for (std::size_t i = 0; i < inputs.size(); ++i) {
      cursors.emplace_back(inputs[i]);
      if (cursors.back().ensureCurrent()) {
        heap.push(HeapEntry{cursors.back().current(), i});
      }
    }

    EventBlock output_block = makeBlock(batch_size);
    while (!control.cancelled() && !heap.empty()) {
      HeapEntry top = heap.top();
      heap.pop();
      output_block.events.push_back(top.ordered_event);

      auto &cursor = cursors[top.stream_index];
      cursor.advance();
      if (cursor.ensureCurrent()) {
        heap.push(HeapEntry{cursor.current(), top.stream_index});
      }

      if (output_block.events.size() == batch_size &&
          !flushBlock(output, output_block, batch_size)) {
        break;
      }
    }

    if (!control.cancelled()) {
      flushBlock(output, output_block, batch_size);
    }
  } catch (...) {
    control.captureCurrentException();
  }
  output->close();
}

DispatchResult dispatchQueue(const BlockQueuePtr &input,
                             const MarketDataEventConsumer &consumer,
                             RunControl &control) {
  DispatchResult result{};
  try {
    EventBlock block;
    while (input->pop(block)) {
      for (const auto &ordered : block.events) {
        const auto &event = ordered.event;
        if (result.total == 0) {
          result.first_ts_recv = event.ts_recv;
        } else if (event.ts_recv < result.last_ts_recv) {
          ++result.out_of_order_ts_recv;
        }
        result.last_ts_recv = event.ts_recv;
        ++result.total;
        consumer(event);
      }
    }
  } catch (...) {
    control.captureCurrentException();
  }
  return result;
}

void joinAll(std::vector<std::thread> &threads) {
  for (auto &thread : threads) {
    if (thread.joinable()) {
      thread.join();
    }
  }
}

} // namespace

const char *mergeStrategyName(MergeStrategy strategy) noexcept {
  switch (strategy) {
  case MergeStrategy::Flat:
    return "flat";
  case MergeStrategy::Hierarchy:
    return "hierarchy";
  }
  return "unknown";
}

std::vector<std::filesystem::path>
listNdjsonFiles(const std::filesystem::path &folder) {
  if (!std::filesystem::exists(folder)) {
    throw std::runtime_error("ingestFolder: path does not exist: " +
                             folder.string());
  }
  if (!std::filesystem::is_directory(folder)) {
    throw std::runtime_error("ingestFolder: expected directory: " +
                             folder.string());
  }

  std::vector<std::filesystem::path> files;
  for (const auto &entry : std::filesystem::directory_iterator(folder)) {
    if (!entry.is_regular_file()) {
      continue;
    }
    const auto ext = entry.path().extension().string();
    if (ext == ".json" || ext == ".ndjson") {
      files.push_back(entry.path());
    }
  }
  std::sort(files.begin(), files.end());
  if (files.empty()) {
    throw std::runtime_error("ingestFolder: no .json or .ndjson files in " +
                             folder.string());
  }
  return files;
}

FolderIngestStats ingestFolder(const std::filesystem::path &folder,
                               MergeStrategy strategy,
                               const MarketDataEventConsumer &consumer,
                               FolderIngestOptions options) {
  if (options.queue_capacity == 0) {
    throw std::invalid_argument(
        "ingestFolder: queue_capacity must be >= 1");
  }
  if (options.batch_size == 0) {
    throw std::invalid_argument("ingestFolder: batch_size must be >= 1");
  }

  const auto files = listNdjsonFiles(folder);
  RunControl control;
  std::vector<std::thread> producer_threads;
  std::vector<std::thread> merger_threads;
  std::vector<ProducerResult> producer_results(files.size());
  std::vector<BlockQueuePtr> producer_queues;
  producer_queues.reserve(files.size());

  for (std::size_t i = 0; i < files.size(); ++i) {
    producer_queues.push_back(makeQueue(options.queue_capacity, control));
  }

  const auto start = Clock::now();

  for (std::size_t i = 0; i < files.size(); ++i) {
    producer_threads.emplace_back(producerThread, files[i],
                                  static_cast<std::uint32_t>(i),
                                  producer_queues[i],
                                  std::ref(producer_results[i]),
                                  options.batch_size, std::ref(control));
  }

  BlockQueuePtr final_queue;
  if (strategy == MergeStrategy::Flat) {
    final_queue = makeQueue(options.queue_capacity, control);
    merger_threads.emplace_back(flatMergerThread, std::cref(producer_queues),
                                final_queue, options.batch_size,
                                std::ref(control));
  } else {
    std::vector<BlockQueuePtr> current = producer_queues;
    while (current.size() > 1) {
      std::vector<BlockQueuePtr> next_level;
      next_level.reserve((current.size() + 1) / 2);
      for (std::size_t i = 0; i < current.size(); i += 2) {
        if (i + 1 == current.size()) {
          next_level.push_back(current[i]);
          continue;
        }
        auto merged = makeQueue(options.queue_capacity, control);
        merger_threads.emplace_back(mergeTwoQueues, current[i], current[i + 1],
                                    merged, options.batch_size,
                                    std::ref(control));
        next_level.push_back(merged);
      }
      current = std::move(next_level);
    }
    final_queue = current.front();
  }

  DispatchResult dispatch{};
  std::thread dispatcher_thread([&] {
    dispatch = dispatchQueue(final_queue, consumer, control);
  });

  joinAll(producer_threads);
  joinAll(merger_threads);
  if (dispatcher_thread.joinable()) {
    dispatcher_thread.join();
  }

  const auto elapsed =
      std::chrono::duration<double>(Clock::now() - start).count();

  control.rethrowIfNeeded();

  FolderIngestStats stats{};
  stats.strategy = strategy;
  stats.files = files.size();
  stats.total = dispatch.total;
  stats.out_of_order_ts_recv = dispatch.out_of_order_ts_recv;
  stats.first_ts_recv = dispatch.first_ts_recv;
  stats.last_ts_recv = dispatch.last_ts_recv;
  stats.elapsed_sec = elapsed;
  for (const auto &producer : producer_results) {
    stats.skipped_rtype += producer.stats.skipped_rtype;
    stats.skipped_parse += producer.stats.skipped_parse;
    stats.producer_out_of_order_ts_recv += producer.stats.out_of_order_ts_recv;
  }
  return stats;
}

} // namespace cmf
