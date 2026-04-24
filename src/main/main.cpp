#include "common/Pipeline.hpp"
#include "data_layer/JsonMarketDataFolderLoader.hpp"
#include "domain/LimitOrderBook.hpp"
#include "domain/Snapshotter.hpp"
#include "transport/Subscriber.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>

namespace {

template <typename QueueT>
void run(const std::string & bench_name,
  const std::string &folder, std::size_t num_threads,
         std::uint64_t snapshot_interval_ns = 1'000'000'000ULL) {
  const auto files = data_layer::discoverJsonFiles(folder);
  const auto queue = std::make_shared<QueueT>(files.size());

  domain::LimitOrderBook<QueueT> lob(num_threads);
  domain::Snapshotter<QueueT> snapshotter(lob, snapshot_interval_ns);

  data_layer::MarketDataFolderLoaderT<QueueT> loader(files, queue);
  transport::QueueSubscriberT<QueueT> subscriber(queue);

  subscriber.addSubscriber({
      .name = "LOB",
      .onEvent = [&](const auto &e) { lob.onEvent(e); },
      .onEndEvents = [&] { lob.onEndEvent(); },
  });
  subscriber.addSubscriber({
      .name = "Snapshotter",
      .onEvent = [&](const auto &e) { snapshotter.onEvent(e); },
      .onEndEvents = [&] { snapshotter.onEndEvents(); },
  });

  const auto t0 = std::chrono::steady_clock::now();

  common::pipeline::runPipeline(loader, subscriber);
  common::pipeline::stopPipeline(subscriber, loader);

  const auto elapsed_us =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::steady_clock::now() - t0)
          .count();

  std::cout << "bench_name=" << bench_name << " ";
  std::cout << "threads=" << num_threads
            << " events=" << snapshotter.eventsSeen()
            << " snapshots=" << snapshotter.snapshots().size()
            << " elapsed_us=" << elapsed_us << '\n';
  for (const auto &snap : snapshotter.snapshots()) {
    std::cout << snap;
  }
  snapshotter.printFinalBestQuotes(std::cout);
}

} // namespace

int main() {
  const std::string folder = "/home/user/Workspace/back-tester-2026/tests_json";
  run<transport::FlatSyncedQueue>("flat_1", folder, 1);
  run<transport::HierarchicalSyncedQueue>("hierarchical_1", folder, 1);
  run<transport::FlatSyncedQueue>("flat_4", folder, 4);
  run<transport::HierarchicalSyncedQueue>("hierarchical_4", folder, 4);

  return 0;
}
