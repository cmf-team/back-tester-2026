#pragma once

#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "data_layer/plain_json_parser/PlainJsonParser.hpp"
#include "transport/FlatSyncedQueue.hpp"
#include "transport/HierarchicalSyncedQueue.hpp"

namespace data_layer {

std::vector<std::string> discoverJsonFiles(const std::string &folder_path);

template <typename QueueT, typename JsonParser = PlainJsonLineParser>
class MarketDataFolderLoaderT final {
public:
  explicit MarketDataFolderLoaderT(const std::vector<std::string> &json_files,
                                   const std::shared_ptr<QueueT> &output_queue)
      : json_files_(json_files) {
    if (json_files_.empty() ||
        output_queue->getMarketEventsQueuesSize() == 0) {
      throw std::invalid_argument(
          "json_files and output_queues must not be empty");
    }

    if (json_files_.size() != output_queue->getMarketEventsQueuesSize()) {
      throw std::invalid_argument(
          "json_files and output_queues must have the same size");
    }

    const auto queues = output_queue->getMarketEventsQueues();
    parsers_.reserve(json_files_.size());
    for (std::size_t i = 0; i < json_files_.size(); ++i) {
      parsers_.emplace_back(json_files_[i], queues[i]);
    }
  }

  ~MarketDataFolderLoaderT() { stop(); }

  MarketDataFolderLoaderT(const MarketDataFolderLoaderT &) = delete;
  MarketDataFolderLoaderT &operator=(const MarketDataFolderLoaderT &) = delete;
  MarketDataFolderLoaderT(MarketDataFolderLoaderT &&) = delete;
  MarketDataFolderLoaderT &operator=(MarketDataFolderLoaderT &&) = delete;

  void run() {
    for (auto &worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
    workers_.clear();
    workers_.reserve(parsers_.size());

    for (auto &parser : parsers_) {
      auto *const worker = std::addressof(parser);
      workers_.emplace_back([worker]() { worker->run(); });
    }
  }

  void stop() {
    for (auto &parser : parsers_) {
      parser.stop();
    }
    for (auto &worker : workers_) {
      if (worker.joinable()) {
        worker.join();
      }
    }
  }

private:
  std::vector<std::string> json_files_;
  std::vector<MarketDataParser<JsonParser>> parsers_;
  std::vector<std::thread> workers_;
};

using MarketDataFolderLoader =
    MarketDataFolderLoaderT<transport::FlatSyncedQueue>;
using HierarchicalMarketDataFolderLoader =
    MarketDataFolderLoaderT<transport::HierarchicalSyncedQueue>;

} 
