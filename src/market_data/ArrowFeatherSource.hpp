// ArrowFeatherSource — IEventSource that reads Databento MBO events from
// an Apache Arrow Feather (IPC) file produced by scripts/convert_to_feather.py.
//
// Compiled only when the project is configured with -DENABLE_ARROW=ON
// (the macro BACKTESTER_HAS_ARROW is set by the build system). Without
// Arrow support this header simply forward-declares the class as a
// non-existent symbol so callers get a clean linker error.

#pragma once

#include "market_data/EventSource.hpp"
#include "market_data/MarketDataEvent.hpp"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <vector>

namespace cmf {

#ifdef BACKTESTER_HAS_ARROW

// Eagerly loads the entire Feather table into memory and serves events
// in row order. Suitable for daily MBO files (~100M rows fit in RAM as
// columnar Arrow arrays). For larger inputs, swap to a streaming
// RecordBatchFileReader iteration; the IEventSource contract does not
// change.
class ArrowFeatherSource final : public IEventSource {
public:
  explicit ArrowFeatherSource(const std::filesystem::path &path);
  ~ArrowFeatherSource() override;

  ArrowFeatherSource(const ArrowFeatherSource &) = delete;
  ArrowFeatherSource &operator=(const ArrowFeatherSource &) = delete;

  bool pop(MarketDataEvent &out) override;

  std::size_t total() const noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Convenience: collect all events from a Feather file into a vector. Useful
// for tests and for round-trip checks against the JSON pipeline.
std::vector<MarketDataEvent>
loadFeatherEvents(const std::filesystem::path &path);

#endif // BACKTESTER_HAS_ARROW

} // namespace cmf
