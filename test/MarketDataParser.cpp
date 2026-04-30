#include "common/MarketDataParser.hpp"
#include "catch2/catch_all.hpp"

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
static const std::string SAMPLE_MBO =
    R"({"ts_recv":"2026-03-09T07:52:41.368148840Z",)"
    R"("hd":{"ts_event":"2026-03-09T07:52:41.367824437Z",)"
    R"("rtype":160,"publisher_id":101,"instrument_id":34513},)"
    R"("action":"A","side":"A","price":"0.021630000","size":20,)"
    R"("channel_id":79,"order_id":"1773042761367855297",)"
    R"("flags":128,"ts_in_delta":2365,"sequence":52012,)"
    R"("symbol":"EUCO SI 20260710 PS EU P 1.1650 0"})";

// ── parseNDJSON - full record
// ─────────────────────────────────────────────────

TEST_CASE("parseNDJSON - sample MBO record fields",
          "[MarketDataEvent][parseNDJSON]")
{
    auto result = parseNDJSON(SAMPLE_MBO);
    REQUIRE(result.has_value());
    const auto& e = *result;

    REQUIRE(e.rtype == RType::Mbo);
    REQUIRE(e.publisher_id == 101);
    REQUIRE(e.instrument_id == 34513);
    REQUIRE(e.action == Action::Add);
    REQUIRE(e.side == Side::Ask);
    REQUIRE(e.price == 21'630'000LL);
    REQUIRE(e.size == 20);
    REQUIRE(e.channel_id == 79);
    REQUIRE(e.order_id == 1773042761367855297ULL);
    REQUIRE(e.flag == Flag::Last);
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
            Flag::Last); // 1 << 7
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 64))->flag ==
            Flag::Tob); // 1 << 6
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 32))->flag ==
            Flag::Snapshot); // 1 << 5
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 16))->flag ==
            Flag::Mbp); // 1 << 4
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 8))->flag ==
            Flag::BadTsRecv); // 1 << 3
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 4))->flag ==
            Flag::MaybeBadBook); // 1 << 2
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 2))->flag ==
            Flag::PublisherSpecific); // 1 << 1
    REQUIRE(parseNDJSON(minimalJson("A", "B", "1.0", 1))->flag ==
            Flag::Reserved); // 1 << 0
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
