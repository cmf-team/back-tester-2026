// Tests for MarketDataEvent and the L3 record decoder.

#include "common/MarketDataEvent.hpp"
#include "ingestion/L3FileReader.hpp"

#include "catch2/catch_all.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

using namespace cmf;

namespace {

// Builds a packed 112-byte TOrderlog record from the given fields, mirroring
// the layout produced by numpy's structured dtype.
std::array<std::uint8_t, kRecordSize> makeRecord() {
  std::array<std::uint8_t, kRecordSize> r{};
  using L = L3RecordLayout;

  const std::int64_t ts_event = 1'700'000'000'123'456'789LL;
  const std::int64_t ts_recv = 1'700'000'000'987'654'321LL;
  const char action = 'A';
  const char side = 'B';
  const double price = 1.23456;
  const std::int64_t size = 42;
  const std::int32_t channel_id = 7;
  const std::uint64_t order_id = 0xCAFEBABEDEADBEEFULL;
  const std::uint8_t flags = 0x80;
  const std::int32_t ts_in_delta = 1234;
  const std::int32_t sequence = 987654;
  const char symbol[] = "FCEU SI 20260316 PS";
  const std::int32_t rtype = 160;
  const std::uint32_t publisher_id = 101;
  const std::int32_t instrument_id = 442;

  std::memcpy(r.data() + L::ts_event, &ts_event, sizeof(ts_event));
  std::memcpy(r.data() + L::ts_recv, &ts_recv, sizeof(ts_recv));
  r[L::action] = static_cast<std::uint8_t>(action);
  r[L::side] = static_cast<std::uint8_t>(side);
  std::memcpy(r.data() + L::price, &price, sizeof(price));
  std::memcpy(r.data() + L::size, &size, sizeof(size));
  std::memcpy(r.data() + L::channel_id, &channel_id, sizeof(channel_id));
  std::memcpy(r.data() + L::order_id, &order_id, sizeof(order_id));
  r[L::flags] = flags;
  std::memcpy(r.data() + L::ts_in_delta, &ts_in_delta, sizeof(ts_in_delta));
  std::memcpy(r.data() + L::sequence, &sequence, sizeof(sequence));
  std::memcpy(r.data() + L::symbol, symbol, sizeof(symbol) - 1);
  std::memcpy(r.data() + L::rtype, &rtype, sizeof(rtype));
  std::memcpy(r.data() + L::publisher_id, &publisher_id, sizeof(publisher_id));
  std::memcpy(r.data() + L::instrument_id, &instrument_id,
              sizeof(instrument_id));
  return r;
}

} // namespace

TEST_CASE("MarketDataEvent - symbol roundtrip", "[MarketDataEvent]") {
  MarketDataEvent ev;
  ev.setSymbol("FCEU SI 20260316 PS");
  REQUIRE(ev.symbolView() == "FCEU SI 20260316 PS");

  ev.setSymbol("");
  REQUIRE(ev.symbolView().empty());
}

TEST_CASE("L3FileReader - decodeRecord parses packed layout",
          "[L3FileReader]") {
  const auto raw = makeRecord();
  MarketDataEvent ev;
  decodeRecord(raw.data(), ev);

  REQUIRE(ev.ts_event == 1'700'000'000'123'456'789LL);
  REQUIRE(ev.ts_recv == 1'700'000'000'987'654'321LL);
  REQUIRE(ev.action == Action::Add);
  REQUIRE(ev.side == Side::Buy);
  REQUIRE(ev.price == Catch::Approx(1.23456));
  REQUIRE(ev.size == 42);
  REQUIRE(ev.channel_id == 7);
  REQUIRE(ev.order_id == 0xCAFEBABEDEADBEEFULL);
  REQUIRE(ev.flags == 0x80);
  REQUIRE(ev.ts_in_delta == 1234);
  REQUIRE(ev.sequence == 987654);
  REQUIRE(ev.symbolView() == "FCEU SI 20260316 PS");
  REQUIRE(ev.rtype == 160);
  REQUIRE(ev.publisher_id == 101u);
  REQUIRE(ev.instrument_id == 442);
}

TEST_CASE("L3FileReader - side / action mapping", "[L3FileReader]") {
  auto raw = makeRecord();
  raw[L3RecordLayout::side] = 'A';
  raw[L3RecordLayout::action] = 'C';
  MarketDataEvent ev;
  decodeRecord(raw.data(), ev);
  REQUIRE(ev.side == Side::Sell);
  REQUIRE(ev.action == Action::Cancel);

  raw[L3RecordLayout::side] = 'N';
  raw[L3RecordLayout::action] = 'R';
  decodeRecord(raw.data(), ev);
  REQUIRE(ev.side == Side::None);
  REQUIRE(ev.action == Action::Clear);
}
