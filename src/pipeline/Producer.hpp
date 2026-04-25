// Producer: a worker thread that reads one NDJSON file via
// FileMarketDataSource and pushes parsed MarketDataEvents into an
// EventQueue. Closes the queue on EOF or fatal error.
//
// Each input file becomes one Producer instance. The Merger then performs
// a k-way merge across all producer queues. This split lets us:
//   * read N files fully in parallel (the bottleneck is JSON parsing,
//     which is purely CPU-bound and scales with cores), and
//   * keep the merge logic file-agnostic.

#pragma once

#include "parser/FileMarketDataSource.hpp"
#include "pipeline/EventQueue.hpp"

#include <atomic>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace cmf {

class Producer {
public:
  Producer(std::filesystem::path path, std::shared_ptr<EventQueue<MarketDataEvent>> out);

  // Non-copyable, non-movable: owns a thread.
  Producer(const Producer &) = delete;
  Producer &operator=(const Producer &) = delete;
  Producer(Producer &&) = delete;
  Producer &operator=(Producer &&) = delete;

  ~Producer();

  void start();
  void join();

  // ---- diagnostics --------------------------------------------------------
  std::uint64_t produced()  const noexcept { return produced_.load(); }
  std::uint64_t skipped()   const noexcept { return skipped_.load(); }
  std::uint64_t malformed() const noexcept { return malformed_.load(); }
  bool          finished()  const noexcept { return finished_.load(); }

  // If the worker died with an exception it is captured here.
  std::exception_ptr error() const noexcept { return error_; }

  const std::filesystem::path &path() const noexcept { return path_; }

private:
  void run();

  std::filesystem::path path_;
  std::shared_ptr<EventQueue<MarketDataEvent>> out_;
  std::thread worker_;

  std::atomic<std::uint64_t> produced_{0};
  std::atomic<std::uint64_t> skipped_{0};
  std::atomic<std::uint64_t> malformed_{0};
  std::atomic<bool>          finished_{false};

  std::exception_ptr error_;
};

} // namespace cmf
