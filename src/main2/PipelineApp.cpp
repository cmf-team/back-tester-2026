#include "main2/PipelineApp.hpp"

#include "lob/LimitOrderBook.hpp"
#include "pipeline/IEventSource.hpp"
#include "pipeline/MergerFlat.hpp"
#include "pipeline/MergerHierarchy.hpp"
#include "pipeline/Producer.hpp"
#include "pipeline/QueueEventSource.hpp"
#include "pipeline/ShardedDispatcher.hpp"
#include "pipeline/Snapshotter.hpp"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <string>

namespace cmf {

std::vector<std::filesystem::path> PipelineConfig::resolveInputFiles() const {
  namespace fs = std::filesystem;
  std::vector<fs::path> files;
  if (fs::is_regular_file(input)) {
    files.push_back(input);
  } else if (fs::is_directory(input)) {
    for (const auto &entry : fs::directory_iterator(input)) {
      if (!entry.is_regular_file())
        continue;
      const auto &p = entry.path();
      const auto ext = p.extension().string();
      // Accept .json, .ndjson, .mbo.json -- effectively anything a Producer
      // can stream line by line.
      if (ext == ".json" || ext == ".ndjson")
        files.push_back(p);
    }
    std::sort(files.begin(), files.end());
  } else {
    throw std::runtime_error("input is neither a file nor a directory: " +
                             input.string());
  }
  if (files.empty())
    throw std::runtime_error("no NDJSON files found at: " + input.string());
  return files;
}

PipelineReport PipelineApp::run(std::ostream &report_out,
                                std::ostream &snapshot_out) {
  const auto files = cfg_.resolveInputFiles();

  if (!cfg_.quiet) {
    report_out << "[pipeline] files=" << files.size()
               << " merger=" << (cfg_.merger == MergerKind::Flat ? "flat" : "hierarchy")
               << " workers=" << cfg_.workers
               << " snapshot_every=" << cfg_.snapshot_every << '\n';
    for (const auto &f : files)
      report_out << "  - " << f.filename().string() << '\n';
  }

  // 1. One queue + one producer per file.
  std::vector<std::shared_ptr<EventQueue<MarketDataEvent>>> qs;
  std::vector<std::unique_ptr<Producer>> producers;
  qs.reserve(files.size());
  producers.reserve(files.size());
  for (const auto &f : files) {
    auto q = std::make_shared<EventQueue<MarketDataEvent>>(cfg_.queue_capacity);
    qs.push_back(q);
    auto p = std::make_unique<Producer>(f, q);
    p->start();
    producers.push_back(std::move(p));
  }

  // 2. Merger over the producer queues.
  std::vector<EventSourcePtr> sources;
  sources.reserve(qs.size());
  for (auto &q : qs)
    sources.push_back(std::make_unique<QueueEventSource>(q));

  EventSourcePtr merger;
  if (cfg_.merger == MergerKind::Flat)
    merger = std::make_unique<MergerFlat>(std::move(sources));
  else
    merger = std::make_unique<MergerHierarchy>(std::move(sources));

  // 3. Dispatcher: sequential or sharded. We give the sequential variant
  //    its own snapshotter; the sharded variant snapshots per-worker via
  //    its hook (see ShardedDispatcher header for caveats).
  PipelineReport rpt;
  using clock = std::chrono::steady_clock;
  const auto t_start = clock::now();

  auto collectBbo = [&](InstrumentId iid, const LimitOrderBook &book) {
    InstrumentBbo b;
    b.iid         = iid;
    b.has_bid     = book.hasBid();
    b.bid_px      = b.has_bid ? book.bestBidPrice() : 0.0;
    b.bid_qty     = b.has_bid ? book.bestBidQty()   : 0;
    b.has_ask     = book.hasAsk();
    b.ask_px      = b.has_ask ? book.bestAskPrice() : 0.0;
    b.ask_qty     = b.has_ask ? book.bestAskQty()   : 0;
    b.bid_levels  = book.bidLevels();
    b.ask_levels  = book.askLevels();
    b.open_orders = book.openOrders();
    rpt.final_bbo.push_back(b);
  };

  if (cfg_.workers == 0) {
    InstrumentBookRegistry reg;
    Snapshotter snap(reg, snapshot_out, cfg_.snapshot_depth);
    if (cfg_.snapshot_every != 0)
      snap.start();

    Dispatcher disp(*merger, reg, cfg_.snapshot_every,
                    [&](std::uint64_t seq, NanoTime ts) {
                      snap.captureAndSubmit(seq, ts);
                    });
    rpt.stats        = disp.run();
    rpt.instruments  = reg.size();
    reg.forEach([&](InstrumentId iid, const LimitOrderBook &book) {
      rpt.total_orders += book.openOrders();
      collectBbo(iid, book);
    });

    snap.stop();
    rpt.snapshots_emitted = snap.emitted();
  } else {
    // Snapshots from multiple workers race for snapshot_out -- guard it.
    std::mutex snap_mu;
    ShardedDispatcher sd(*merger, cfg_.workers, cfg_.snapshot_every,
        [&](std::size_t worker, const InstrumentBookRegistry &reg,
            std::uint64_t seq, NanoTime ts) {
          std::lock_guard<std::mutex> lk(snap_mu);
          snapshot_out << "[snapshot worker=" << worker
                       << " seq=" << seq
                       << " ts=" << formatIso8601Nanos(ts)
                       << " instruments=" << reg.size() << "]\n";
          reg.forEach([&](InstrumentId iid, const LimitOrderBook &book) {
            snapshot_out << "  iid=" << iid
                         << " bids=" << book.bidLevels()
                         << " asks=" << book.askLevels()
                         << " orders=" << book.openOrders();
            if (book.hasBid())
              snapshot_out << " top_bid=" << book.bestBidPrice() << '@'
                           << book.bestBidQty();
            if (book.hasAsk())
              snapshot_out << " top_ask=" << book.bestAskPrice() << '@'
                           << book.bestAskQty();
            snapshot_out << '\n';
          });
        });
    rpt.stats = sd.run();
    for (std::size_t i = 0; i < sd.numWorkers(); ++i) {
      rpt.instruments += sd.registry(i).size();
      sd.registry(i).forEach([&](InstrumentId iid, const LimitOrderBook &book) {
        rpt.total_orders += book.openOrders();
        collectBbo(iid, book);
      });
    }
  }
  std::sort(rpt.final_bbo.begin(), rpt.final_bbo.end(),
            [](const InstrumentBbo &a, const InstrumentBbo &b) {
              return a.iid < b.iid;
            });

  const auto t_end = clock::now();
  rpt.wall_seconds =
      std::chrono::duration<double>(t_end - t_start).count();
  rpt.events_per_sec = rpt.wall_seconds > 0.0
                           ? static_cast<double>(rpt.stats.events_in) /
                                 rpt.wall_seconds
                           : 0.0;

  // 4. Producer accounting.
  for (auto &p : producers) {
    p->join();
    rpt.produced_total  += p->produced();
    rpt.malformed_total += p->malformed();
    if (auto err = p->error()) {
      try { std::rethrow_exception(err); }
      catch (const std::exception &e) {
        report_out << "[producer error] " << p->path().filename() << ": "
                   << e.what() << '\n';
      }
    }
  }

  // 5. Final report.
  report_out << "\n=== Pipeline report ===\n";
  report_out << "events_in       = " << rpt.stats.events_in       << '\n';
  report_out << "events_routed   = " << rpt.stats.events_routed   << '\n';
  report_out << "events_unknown  = " << rpt.stats.events_unknown  << '\n';
  report_out << "events_unroute  = " << rpt.stats.events_unroutable << '\n';
  report_out << "instruments     = " << rpt.instruments           << '\n';
  report_out << "open_orders     = " << rpt.total_orders          << '\n';
  report_out << "snapshots       = " << rpt.snapshots_emitted     << '\n';
  report_out << "produced_total  = " << rpt.produced_total        << '\n';
  report_out << "malformed_total = " << rpt.malformed_total       << '\n';
  if (rpt.stats.events_in > 0) {
    report_out << "first_ts        = " << formatIso8601Nanos(rpt.stats.first_ts)
               << '\n';
    report_out << "last_ts         = " << formatIso8601Nanos(rpt.stats.last_ts)
               << '\n';
  }

  // Performance ----------------------------------------------------------
  report_out << "\n=== Performance ===\n";
  report_out << std::fixed << std::setprecision(3)
             << "wall_seconds    = " << rpt.wall_seconds        << '\n'
             << std::setprecision(0)
             << "events_per_sec  = " << rpt.events_per_sec      << '\n';

  if (!rpt.final_bbo.empty()) {
    report_out << "\n=== Final best bid/ask per instrument ===\n";
    for (const auto &b : rpt.final_bbo) {
      report_out << "iid=" << b.iid;
      if (b.has_bid)
        report_out << " bid=" << std::fixed << std::setprecision(5)
                   << b.bid_px << '@' << b.bid_qty;
      else
        report_out << " bid=-";
      if (b.has_ask)
        report_out << " ask=" << std::fixed << std::setprecision(5)
                   << b.ask_px << '@' << b.ask_qty;
      else
        report_out << " ask=-";
      report_out << " bid_lvls=" << b.bid_levels
                 << " ask_lvls=" << b.ask_levels
                 << " orders="   << b.open_orders << '\n';
    }
  }

  return rpt;
}

} // namespace cmf
