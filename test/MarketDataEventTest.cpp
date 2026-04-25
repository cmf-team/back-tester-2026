#include "common/MarketDataEvent.hpp"
#include "common/MarketDataParser.hpp"

#include "catch2/catch_all.hpp"

#include <cstdint>
#include <limits>
#include <string>

// ── Helpers
// ───────────────────────────────────────────────────────────────────

// Builds a minimal valid MBO JSON line with overridable fields.
static std::string minimalJson(const std::string& action = "A",
                               const std::string& side = "B",
                               const std::string& price = "1.000000000",
                               int flags = 0, int ts_in_delta = 0)
{
    return R"({"ts_recv":"2026-03-09T07:52:41.368148840Z",)"
           R"("hd":{"ts_event":"2026-03-09T07:52:41.367824437Z",)"
           R"("rtype":160,"publisher_id":101,"instrument_id":34513},)"
           R"("action":")" +
           action + R"(","side":")" + side + R"(","price":")" + price +
           R"(","size":1,"channel_id":1,"order_id":"42","flags":)" +
           std::to_string(flags) + R"(,"ts_in_delta":)" +
           std::to_string(ts_in_delta) + "}";
}

// Sample taken verbatim from the MarketDataEvent.hpp header comment.
static const char* const SAMPLE_MBO =
    R"({"ts_recv":"2026-03-09T07:52:41.368148840Z",)"
    R"("hd":{"ts_event":"2026-03-09T07:52:41.367824437Z",)"
    R"("rtype":160,"publisher_id":101,"instrument_id":34513},)"
    R"("action":"A","side":"A","price":"0.021630000","size":20,)"
    R"("channel_id":79,"order_id":"1773042761367855297",)"
    R"("flags":128,"ts_in_delta":2365,"sequence":52012,)"
    R"("symbol":"EUCO SI 20260710 PS EU P 1.1650 0"})";

// ── Constants
// ─────────────────────────────────────────────────────────────────

TEST_CASE("MarketDataEvent - sentinel constants", "[MarketDataEvent]")
{
    REQUIRE(MarketDataEvent::UNDEF_PRICE ==
            std::numeric_limits<std::int64_t>::max());
    REQUIRE(MarketDataEvent::UNDEF_TIMESTAMP ==
            std::numeric_limits<std::uint64_t>::max());
}

// ── priceToDouble
// ─────────────────────────────────────────────────────────────

TEST_CASE("MarketDataEvent::priceToDouble", "[MarketDataEvent]")
{
    REQUIRE(MarketDataEvent::priceToDouble(0) == 0.0);

    // 1 unit = 1e-9 (fixed-point scale)
    REQUIRE(MarketDataEvent::priceToDouble(1'000'000'000LL) ==
            Catch::Approx(1.0));

    // From Databento docs: 5411750000000 → 5411.75
    REQUIRE(MarketDataEvent::priceToDouble(5'411'750'000'000LL) ==
            Catch::Approx(5411.75));

    // Negative prices are valid for calendar spreads (per Databento docs)
    REQUIRE(MarketDataEvent::priceToDouble(-1'000'000'000LL) ==
            Catch::Approx(-1.0));

    // Typical small options price: 0.02163
    REQUIRE(MarketDataEvent::priceToDouble(21'630'000LL) ==
            Catch::Approx(0.02163));
}

// ── sideToString ─────────────────────────────────────────────────────────────

TEST_CASE("MarketDataEvent::sideToString", "[MarketDataEvent]")
{
    REQUIRE(MarketDataEvent::sideToString(Side::Bid) == "Bid");
    REQUIRE(MarketDataEvent::sideToString(Side::Ask) == "Ask");
    REQUIRE(MarketDataEvent::sideToString(Side::None) == "None");
}

// ── actionToString
// ────────────────────────────────────────────────────────────

TEST_CASE("MarketDataEvent::actionToString", "[MarketDataEvent]")
{
    REQUIRE(MarketDataEvent::actionToString(Action::Add) == "Add");
    REQUIRE(MarketDataEvent::actionToString(Action::Modify) == "Modify");
    REQUIRE(MarketDataEvent::actionToString(Action::Cancel) == "Cancel");
    REQUIRE(MarketDataEvent::actionToString(Action::Clear) == "Clear");
    REQUIRE(MarketDataEvent::actionToString(Action::Trade) == "Trade");
    REQUIRE(MarketDataEvent::actionToString(Action::Fill) == "Fill");
    REQUIRE(MarketDataEvent::actionToString(Action::None) == "None");
}

// ── flagToString
// ──────────────────────────────────────────────────────────────

TEST_CASE("MarketDataEvent::flagToString", "[MarketDataEvent]")
{
    REQUIRE(MarketDataEvent::flagToString(Flag::None) == "None");
    REQUIRE(MarketDataEvent::flagToString(Flag::F_LAST) == "F_LAST");
    REQUIRE(MarketDataEvent::flagToString(Flag::F_TOB) == "F_TOB");
    REQUIRE(MarketDataEvent::flagToString(Flag::F_SNAPSHOT) == "F_SNAPSHOT");
    REQUIRE(MarketDataEvent::flagToString(Flag::F_MBP) == "F_MBP");
    REQUIRE(MarketDataEvent::flagToString(Flag::F_BAD_TS_RECV) ==
            "F_BAD_TS_RECV");
    REQUIRE(MarketDataEvent::flagToString(Flag::F_MAYBE_BAD_BOOK) ==
            "F_MAYBE_BAD_BOOK");
    REQUIRE(MarketDataEvent::flagToString(Flag::F_PUBLISHER_SPECIFIC) ==
            "F_PUBLISHER_SPECIFIC");
    REQUIRE(MarketDataEvent::flagToString(Flag::F_RESERVED) == "F_RESERVED");
}

// ── rTypeToString
// ─────────────────────────────────────────────────────────────

TEST_CASE("MarketDataEvent::rTypeToString", "[MarketDataEvent]")
{
    REQUIRE(MarketDataEvent::rTypeToString(RType::MBP_0) == "MBP_0");
    REQUIRE(MarketDataEvent::rTypeToString(RType::MBP_1) == "MBP_1");
    REQUIRE(MarketDataEvent::rTypeToString(RType::MBP_10) == "MBP_10");
    REQUIRE(MarketDataEvent::rTypeToString(RType::Status) == "Status");
    REQUIRE(MarketDataEvent::rTypeToString(RType::Definition) == "Definition");
    REQUIRE(MarketDataEvent::rTypeToString(RType::Imbalance) == "Imbalance");
    REQUIRE(MarketDataEvent::rTypeToString(RType::Error) == "Error");
    REQUIRE(MarketDataEvent::rTypeToString(RType::SymbolMapping) ==
            "SymbolMapping");
    REQUIRE(MarketDataEvent::rTypeToString(RType::System) == "System");
    REQUIRE(MarketDataEvent::rTypeToString(RType::Statistics) == "Statistics");
    REQUIRE(MarketDataEvent::rTypeToString(RType::OHLCV_1s) == "OHLCV_1s");
    REQUIRE(MarketDataEvent::rTypeToString(RType::OHLCV_1m) == "OHLCV_1m");
    REQUIRE(MarketDataEvent::rTypeToString(RType::OHLCV_1h) == "OHLCV_1h");
    REQUIRE(MarketDataEvent::rTypeToString(RType::OHLCV_1d) == "OHLCV_1d");
    REQUIRE(MarketDataEvent::rTypeToString(RType::MBO) == "MBO");
    REQUIRE(MarketDataEvent::rTypeToString(RType::CMBP_1) == "CMBP_1");
    REQUIRE(MarketDataEvent::rTypeToString(RType::CBBO_1s) == "CBBO_1s");
    REQUIRE(MarketDataEvent::rTypeToString(RType::CBBO_1m) == "CBBO_1m");
    REQUIRE(MarketDataEvent::rTypeToString(RType::TCBBO) == "TCBBO");
    REQUIRE(MarketDataEvent::rTypeToString(RType::BBO_1s) == "BBO_1s");
    REQUIRE(MarketDataEvent::rTypeToString(RType::BBO_1m) == "BBO_1m");
}

// ── Default construction
// ──────────────────────────────────────────────────────

TEST_CASE("MarketDataEvent - default field values", "[MarketDataEvent]")
{
    MarketDataEvent e{};
    REQUIRE(e.sort_ts == MarketDataEvent::UNDEF_TIMESTAMP);
    REQUIRE(e.ts_recv == MarketDataEvent::UNDEF_TIMESTAMP);
    REQUIRE(e.ts_event == MarketDataEvent::UNDEF_TIMESTAMP);
    REQUIRE(e.rtype == RType::MBP_0);
    REQUIRE(e.publisher_id == 0);
    REQUIRE(e.instrument_id == 0);
    REQUIRE(e.action == Action::None);
    REQUIRE(e.side == Side::None);
    REQUIRE(e.price == MarketDataEvent::UNDEF_PRICE);
    REQUIRE(e.size == 0);
    REQUIRE(e.channel_id == 0);
    REQUIRE(e.order_id == 0);
    REQUIRE(e.flag == Flag::None);
    REQUIRE(e.ts_in_delta == 0);
    REQUIRE(e.source_file_id == 0);
}

// ── parseNDJSON - full record
// ─────────────────────────────────────────────────

TEST_CASE("parseNDJSON - sample MBO record fields",
          "[MarketDataEvent][parseNDJSON]")
{
    auto result = parseNDJSON(SAMPLE_MBO);
    REQUIRE(result.has_value());
    const auto& e = *result;

    REQUIRE(e.rtype == RType::MBO);
    REQUIRE(e.publisher_id == 101);
    REQUIRE(e.instrument_id == 34513);
    REQUIRE(e.action == Action::Add);
    REQUIRE(e.side == Side::Ask);
    REQUIRE(e.price == 21'630'000LL);
    REQUIRE(e.size == 20);
    REQUIRE(e.channel_id == 79);
    REQUIRE(e.order_id == 1773042761367855297ULL);
    REQUIRE(e.flag == Flag::F_LAST);
    REQUIRE(e.ts_in_delta == 2365);
}

TEST_CASE("parseNDJSON - timestamps are parsed from ISO 8601",
          "[MarketDataEvent][parseNDJSON]")
{
    auto result = parseNDJSON(SAMPLE_MBO);
    REQUIRE(result.has_value());
    const auto& e = *result;

    REQUIRE(e.ts_recv != MarketDataEvent::UNDEF_TIMESTAMP);
    REQUIRE(e.ts_event != MarketDataEvent::UNDEF_TIMESTAMP);
    // ts_recv is later than ts_event for this record
    REQUIRE(e.ts_recv > e.ts_event);
}

// Per Databento spec: sort_ts = ts_recv when ts_recv is present in the record.
TEST_CASE("parseNDJSON - sort_ts equals ts_recv when ts_recv is present",
          "[MarketDataEvent][parseNDJSON]")
{
    auto result = parseNDJSON(SAMPLE_MBO);
    REQUIRE(result.has_value());
    const auto& e = *result;
    REQUIRE(e.sort_ts == e.ts_recv);
}

// ── parseNDJSON - action codes
// ────────────────────────────────────────────────

TEST_CASE("parseNDJSON - action code mapping",
          "[MarketDataEvent][parseNDJSON]")
{
    // Databento action codes: A=Add M=Modify C=Cancel R=Clear T=Trade F=Fill
    // N=None
    REQUIRE(parseNDJSON(minimalJson("A"))->action == Action::Add);
    REQUIRE(parseNDJSON(minimalJson("M"))->action == Action::Modify);
    REQUIRE(parseNDJSON(minimalJson("C"))->action == Action::Cancel);
    REQUIRE(parseNDJSON(minimalJson("R"))->action == Action::Clear);
    REQUIRE(parseNDJSON(minimalJson("T"))->action == Action::Trade);
    REQUIRE(parseNDJSON(minimalJson("F"))->action == Action::Fill);
    REQUIRE(parseNDJSON(minimalJson("N"))->action == Action::None);
}

// ── parseNDJSON - side codes
// ──────────────────────────────────────────────────

TEST_CASE("parseNDJSON - side code mapping", "[MarketDataEvent][parseNDJSON]")
{
    // Databento side codes: B=Bid (buy), A=Ask (sell)
    REQUIRE(parseNDJSON(minimalJson("A", "B"))->side == Side::Bid);
    REQUIRE(parseNDJSON(minimalJson("A", "A"))->side == Side::Ask);
}

// When action is Clear, side is always N per Databento spec.
TEST_CASE("parseNDJSON - side N maps to None",
          "[MarketDataEvent][parseNDJSON]")
{
    auto result = parseNDJSON(minimalJson("R", "N"));
    REQUIRE(result.has_value());
    REQUIRE(result->side == Side::None);
}

// ── parseNDJSON - price conversion
// ────────────────────────────────────────────

TEST_CASE("parseNDJSON - price string to fixed-point int64",
          "[MarketDataEvent][parseNDJSON]")
{
    // From Databento docs: decimal → integer scaled by 1e9
    REQUIRE(parseNDJSON(minimalJson("A", "B", "5411.750000000"))->price ==
            5'411'750'000'000LL);
    REQUIRE(parseNDJSON(minimalJson("A", "B", "0.000000000"))->price == 0LL);
    REQUIRE(parseNDJSON(minimalJson("A", "B", "0.021630000"))->price ==
            21'630'000LL);
    // Negative prices are valid (calendar spreads per Databento docs)
    REQUIRE(parseNDJSON(minimalJson("A", "B", "-1.000000000"))->price ==
            -1'000'000'000LL);
}

// ── parseNDJSON - ts_in_delta
// ─────────────────────────────────────────────────

TEST_CASE("parseNDJSON - ts_in_delta can be negative",
          "[MarketDataEvent][parseNDJSON]")
{
    // ts_in_delta is int32 and may be negative when publisher/Databento clocks
    // differ
    auto result = parseNDJSON(minimalJson("T", "A", "1.000000000", 0, -5000));
    REQUIRE(result.has_value());
    REQUIRE(result->ts_in_delta == -5000);
}

TEST_CASE("parseNDJSON - ts_in_delta positive",
          "[MarketDataEvent][parseNDJSON]")
{
    auto result = parseNDJSON(minimalJson("A", "B", "1.000000000", 0, 2365));
    REQUIRE(result.has_value());
    REQUIRE(result->ts_in_delta == 2365);
}

// ── parseNDJSON - flag bit field
// ──────────────────────────────────────────────

TEST_CASE("parseNDJSON - flag values", "[MarketDataEvent][parseNDJSON]")
{
    // Each flag is a single bit per Databento spec
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 0))->flag == Flag::None);
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 128))->flag ==
            Flag::F_LAST); // 1 << 7
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 64))->flag ==
            Flag::F_TOB); // 1 << 6
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 32))->flag ==
            Flag::F_SNAPSHOT); // 1 << 5
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 16))->flag ==
            Flag::F_MBP); // 1 << 4
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 8))->flag ==
            Flag::F_BAD_TS_RECV); // 1 << 3
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 4))->flag ==
            Flag::F_MAYBE_BAD_BOOK); // 1 << 2
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 2))->flag ==
            Flag::F_PUBLISHER_SPECIFIC); // 1 << 1
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 1))->flag ==
            Flag::F_RESERVED); // 1 << 0
}

// ── parseNDJSON - invalid input
// ───────────────────────────────────────────────

TEST_CASE("parseNDJSON - malformed input returns nullopt",
          "[MarketDataEvent][parseNDJSON]")
{
    REQUIRE_FALSE(parseNDJSON("").has_value());
    REQUIRE_FALSE(parseNDJSON("not json at all").has_value());
    REQUIRE_FALSE(parseNDJSON("{}").has_value());
    REQUIRE_FALSE(parseNDJSON("null").has_value());
    REQUIRE_FALSE(parseNDJSON("[1,2,3]").has_value());
}

TEST_CASE("parseNDJSON - missing required fields return nullopt",
          "[MarketDataEvent][parseNDJSON]")
{
    // missing "hd"
    REQUIRE_FALSE(
        parseNDJSON(
            R"({"ts_recv":"2026-03-09T07:52:41.368148840Z","action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0})")
            .has_value());

    // missing "action"
    REQUIRE_FALSE(
        parseNDJSON(
            R"({"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0})")
            .has_value());

    // missing "side"
    REQUIRE_FALSE(
        parseNDJSON(
            R"({"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0})")
            .has_value());

    // missing "price"
    REQUIRE_FALSE(
        parseNDJSON(
            R"({"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","side":"B","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0})")
            .has_value());

    // missing "order_id"
    REQUIRE_FALSE(
        parseNDJSON(
            R"({"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"flags":0,"ts_in_delta":0})")
            .has_value());
}
