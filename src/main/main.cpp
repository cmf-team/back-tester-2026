#include "common/Pipeline.hpp"
#include "data_layer/JsonMarketDataFolderLoader.hpp"
#include "transport/Subscriber.hpp"
#include <common/LimitOrderBook.hpp>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

template <typename QueueT>
void runPipeline(const std::vector<std::string> &json_files) {
  const auto queue = std::make_shared<QueueT>(json_files.size());
  domain::LimitOrderBook<QueueT> limit_order_book(4);

  {
    data_layer::MarketDataFolderLoaderT<QueueT> folder_loader(json_files,
                                                              queue);
    transport::QueueSubscriberT<QueueT> subscriber(queue);

    subscriber.addSubscriber({
        .name = "LOB",
        .onEvent =
            [&](const domain::events::MarketDataEvent &event) {
              limit_order_book.onEvent(event);
            },
        .onEndEvents = [&]() { limit_order_book.onEndEvent(); },
    });

    common::pipeline::runPipeline(folder_loader, subscriber);
    common::pipeline::stopPipeline(subscriber, folder_loader);

    // implement logging LOB
    auto bids = limit_order_book.getAllBids();
    auto asks = limit_order_book.getAllAsks();
    std::cout << "Actual LOB Bids:\n";
    for (const auto &[instrument_id, bids] : bids) {
      std::cout << "Instrument " << instrument_id << ":\n";
      for (const auto &[price, quantity] : bids) {
        std::cout << "  bids: {" << price << ": " << quantity << "}\n";
      }
    }
    std::cout << "Actual LOB Asks:\n";
    for (const auto &[instrument_id, asks] : asks) {
      std::cout << "Instrument " << instrument_id << ":\n";
      for (const auto &[price, quantity] : asks) {
        std::cout << "  asks: {" << price << ": " << quantity << "}\n";
      }
    }
  }
}

} // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[]) {
  namespace fs = std::filesystem;
  const fs::path tests_path =
      "/home/evo/Workspace/cmf/back-tester-2026/tests_json";
  std::vector<std::string> json_files;

  try {
    if (!fs::is_directory(tests_path)) {
      std::cerr << "Expected tests_json directory: " << tests_path << '\n';
      return 1;
    }
    json_files = data_layer::discoverJsonFiles(tests_path.string());
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }

  if (json_files.empty()) {
    std::cerr << "No .mbo.json files to process in " << tests_path << '\n';
    return 1;
  }

  std::cout << "Using JSON files from: " << tests_path << '\n';
  for (const auto &file : json_files) {
    std::cout << "  - " << file << '\n';
  }

  runPipeline<transport::FlatSyncedQueue>(json_files);

  std::cout
      << "\nExpected final LOB state for quick debug (L3 order-id based):\n";
  std::cout << "Instrument 910001:\n";
  std::cout << "  bids: {100.000000000: 12, 99.500000000: 8}\n";
  std::cout << "  asks: {102.000000000: 25, 103.000000000: 10}\n";
  std::cout << "Instrument 910002:\n";
  std::cout << "  bids: {249.000000000: 20}\n";
  std::cout << "  asks: {251.000000000: 15}\n";
  std::cout << "\nDone\n";

  return 0;
}
