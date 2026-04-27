#include "TempFile.hpp"
#include "catch2/catch_all.hpp"
#include "ingestion/FeatherDataParser.hpp"
#include "ingestion/JsonNativeDataParser.hpp"
#include "ingestion/JsonSimpleDataParser.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace cmf;

// 2026-04-01T00:00:00Z = Unix second 1775001600
static constexpr int64_t BASE_SEC = 1775001600LL;
static constexpr int64_t NS = 1'000'000'000LL;

TEMPLATE_TEST_CASE("DataParser - parses 3 JSON lines", "[DataParser]",
                   JsonSimpleDataParser, JsonNativeDataParser)
{
    TempFile tmp("dataparser_test.mbo.json");
    {
        std::ofstream fs(tmp.getPath());
        fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"A","price":"1.161100000","size":20,"channel_id":23,"order_id":"1775001621480460739","flags":128,"ts_in_delta":1041,"sequence":120067,"symbol":"FCEU SI 20260615 PS"})"
           << "\n";
        fs << R"({"ts_recv":"2026-04-01T00:00:21.480541935Z","hd":{"ts_event":"2026-04-01T00:00:21.480529509Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"C","side":"A","price":"1.161200000","size":20,"channel_id":23,"order_id":"1775001620423626831","flags":128,"ts_in_delta":1056,"sequence":120070,"symbol":"FCEU SI 20260615 PS"})"
           << "\n";
        fs << R"({"ts_recv":"2026-04-01T00:00:23.720604803Z","hd":{"ts_event":"2026-04-01T00:00:23.720590379Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"C","side":"B","price":"1.160700000","size":20,"channel_id":23,"order_id":"10998373657385341895","flags":128,"ts_in_delta":1538,"sequence":120283,"symbol":"FCEU SI 20260615 PS"})"
           << "\n";
    }

    std::vector<MarketDataEvent> events;
    TestType parser(tmp.getPath());
    parser.parse([&](const MarketDataEvent& e)
                 { events.push_back(e); });

    REQUIRE(events.size() == 3);

    // Event 0: action A, side A (Ask/Sell)
    {
        const auto& e = events[0];
        REQUIRE(e.ts_recv == (BASE_SEC + 21) * NS + 480'470'598LL);
        REQUIRE(e.ts_event == (BASE_SEC + 21) * NS + 480'454'469LL);
        REQUIRE(e.action == Action::Add);
        REQUIRE(e.side == Side::Sell);
        REQUIRE(e.order_id == 1775001621480460739ULL);
        REQUIRE(e.instrument_id == 436);
        REQUIRE(e.publisher_id == 101);
        REQUIRE(e.size == 20);
        REQUIRE(e.channel_id == 23);
        REQUIRE(e.rtype == RType::Mbo);
        REQUIRE(e.flags == Flags::Last);
        REQUIRE(e.ts_in_delta == 1041);
        REQUIRE(e.sequence == 120067);
        REQUIRE(e.is_price_defined());
        REQUIRE(e.price_as_double() == Catch::Approx(1.1611).epsilon(1e-4));
    }

    // Event 1: action C, side A (Ask/Sell)
    {
        const auto& e = events[1];
        REQUIRE(e.ts_recv == (BASE_SEC + 21) * NS + 480'541'935LL);
        REQUIRE(e.action == Action::Cancel);
        REQUIRE(e.side == Side::Sell);
        REQUIRE(e.order_id == 1775001620423626831ULL);
        REQUIRE(e.sequence == 120070);
    }

    // Event 2: action C, side B (Bid/Buy)
    {
        const auto& e = events[2];
        REQUIRE(e.ts_recv == (BASE_SEC + 23) * NS + 720'604'803LL);
        REQUIRE(e.ts_event == (BASE_SEC + 23) * NS + 720'590'379LL);
        REQUIRE(e.action == Action::Cancel);
        REQUIRE(e.side == Side::Buy);
        REQUIRE(e.order_id == 10998373657385341895ULL);
        REQUIRE(e.ts_in_delta == 1538);
        REQUIRE(e.sequence == 120283);
    }
}

TEMPLATE_TEST_CASE("DataParser - skips malformed lines", "[DataParser]",
                   JsonSimpleDataParser, JsonNativeDataParser)
{
    TempFile tmp("dataparser_bad.mbo.json");
    {
        std::ofstream fs(tmp.getPath());
        fs << "not json at all\n";
        fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"A","price":"1.161100000","size":20,"channel_id":23,"order_id":"1775001621480460739","flags":128,"ts_in_delta":1041,"sequence":120067,"symbol":"X"})"
           << "\n";
    }

    std::vector<MarketDataEvent> events;
    TestType parser(tmp.getPath());
    parser.parse([&](const MarketDataEvent& e)
                 { events.push_back(e); });

    REQUIRE(events.size() == 1);
}

TEMPLATE_TEST_CASE("DataParser - empty file sets empty flag", "[DataParser]",
                   JsonSimpleDataParser, JsonNativeDataParser)
{
    TempFile tmp("dataparser_empty.mbo.json");
    {
        std::ofstream fs(tmp.getPath());
    }

    std::vector<MarketDataEvent> events;
    TestType parser(tmp.getPath());
    parser.parse([&](const MarketDataEvent& e)
                 { events.push_back(e); });

    REQUIRE(events.empty());
}

TEMPLATE_TEST_CASE("DataParser - parses negative price", "[DataParser]",
                   JsonSimpleDataParser, JsonNativeDataParser)
{
    TempFile tmp("dataparser_negprice.mbo.json");
    {
        std::ofstream fs(tmp.getPath());
        fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"B","price":"-0.250000000","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":1,"symbol":"OPT"})"
           << "\n";
        fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"B","price":"-5.000000000","size":1,"channel_id":1,"order_id":"2","flags":0,"ts_in_delta":0,"sequence":2,"symbol":"OPT"})"
           << "\n";
    }

    std::vector<MarketDataEvent> events;
    TestType parser(tmp.getPath());
    parser.parse([&](const MarketDataEvent& e)
                 { events.push_back(e); });

    REQUIRE(events.size() == 2);
    REQUIRE(events[0].price == -250'000'000LL);   // -0.25 * 1e9
    REQUIRE(events[1].price == -5'000'000'000LL); // -5.0 * 1e9
}

TEMPLATE_TEST_CASE("DataParser - parses side correctly (Buy/Sell/None)",
                   "[DataParser]", JsonSimpleDataParser, JsonNativeDataParser)
{
    TempFile tmp("dataparser_sides.mbo.json");
    {
        std::ofstream fs(tmp.getPath());
        // 'B' = Bid = Buy
        fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"B","price":"1.161100000","size":20,"channel_id":23,"order_id":"1","flags":128,"ts_in_delta":1041,"sequence":1,"symbol":"TEST"})"
           << "\n";
        // 'S' = Sale = Sell
        fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"S","price":"1.161200000","size":30,"channel_id":23,"order_id":"2","flags":128,"ts_in_delta":1041,"sequence":2,"symbol":"TEST"})"
           << "\n";
        // 'A' = Ask = Sell (alternative convention)
        fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"A","price":"1.161300000","size":25,"channel_id":23,"order_id":"3","flags":128,"ts_in_delta":1041,"sequence":3,"symbol":"TEST"})"
           << "\n";
        // 'N' = None
        fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"N","price":"1.161400000","size":10,"channel_id":23,"order_id":"4","flags":128,"ts_in_delta":1041,"sequence":4,"symbol":"TEST"})"
           << "\n";
    }

    std::vector<MarketDataEvent> events;
    TestType parser(tmp.getPath());
    parser.parse([&](const MarketDataEvent& e)
                 { events.push_back(e); });

    REQUIRE(events.size() == 4);

    // 'B' = Bid = Buy
    REQUIRE(events[0].side == Side::Buy);
    REQUIRE(events[0].size == 20);

    // 'S' = Sale = Sell
    REQUIRE(events[1].side == Side::Sell);
    REQUIRE(events[1].size == 30);

    // 'A' = Ask = Sell
    REQUIRE(events[2].side == Side::Sell);
    REQUIRE(events[2].size == 25);

    // 'N' = None
    REQUIRE(events[3].side == Side::None);
    REQUIRE(events[3].size == 10);
}

TEMPLATE_TEST_CASE("DataParser - rejects non-.mbo.json files", "[DataParser]",
                   JsonSimpleDataParser, JsonNativeDataParser)
{
    TempFile tmp("dataparser_invalid.json");
    {
        std::ofstream fs(tmp.getPath());
        fs << R"({"ts_recv":"2026-04-01T00:00:21.480470598Z","hd":{"ts_event":"2026-04-01T00:00:21.480454469Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"B"})"
           << "\n";
    }

    TestType parser(tmp.getPath());
    REQUIRE_THROWS_AS(parser.parse([](const MarketDataEvent&) {}),
                      std::runtime_error);
    REQUIRE_THROWS_WITH(parser.parse([](const MarketDataEvent&) {}),
                        Catch::Matchers::ContainsSubstring("unsupported format"));
}

TEMPLATE_TEST_CASE("DataParser - rejects .csv files", "[DataParser]",
                   JsonSimpleDataParser, JsonNativeDataParser)
{
    TempFile tmp("dataparser_invalid.csv");
    {
        std::ofstream fs(tmp.getPath());
        fs << "ts_recv,side,price\n";
        fs << "2026-04-01T00:00:21.480470598Z,B,1.1611\n";
    }

    TestType parser(tmp.getPath());
    REQUIRE_THROWS_AS(parser.parse([](const MarketDataEvent&) {}),
                      std::runtime_error);
}

TEMPLATE_TEST_CASE("DataParser - rejects .txt files", "[DataParser]",
                   JsonSimpleDataParser, JsonNativeDataParser)
{
    TempFile tmp("dataparser_invalid.txt");
    {
        std::ofstream fs(tmp.getPath());
        fs << "some market data\n";
    }

    TestType parser(tmp.getPath());
    REQUIRE_THROWS_AS(parser.parse([](const MarketDataEvent&) {}),
                      std::runtime_error);
}

TEST_CASE("FeatherDataParser - parses all fixture files (3 rows each)",
          "[FeatherDataParser]")
{
    auto fixtures_dir = std::filesystem::path(FIXTURES_DIR);
    std::vector<std::filesystem::path> feather_files;

    for (const auto& entry : std::filesystem::directory_iterator(fixtures_dir))
    {
        if (entry.is_regular_file() && entry.path().extension() == ".feather")
        {
            feather_files.push_back(entry.path());
        }
    }

    REQUIRE(feather_files.size() > 0);

    for (const auto& fixture_path : feather_files)
    {
        std::vector<MarketDataEvent> events;
        FeatherDataParser parser(fixture_path);
        parser.parse([&](const MarketDataEvent& e)
                     { events.push_back(e); });

        REQUIRE(events.size() == 3);

        for (const auto& e : events)
        {
            REQUIRE(e.ts_recv > 0);
            REQUIRE(e.ts_event > 0);
            REQUIRE((e.side == Side::Buy || e.side == Side::Sell ||
                     e.side == Side::None));
        }
    }
}

TEST_CASE("FeatherDataParser - rejects non-.mbo.json.feather files",
          "[FeatherDataParser]")
{
    TempFile tmp("bad.json");
    {
        std::ofstream fs(tmp.getPath());
        fs << "not a feather file\n";
    }

    FeatherDataParser parser(tmp.getPath());
    REQUIRE_THROWS_WITH(parser.parse([](const MarketDataEvent&) {}),
                        Catch::Matchers::ContainsSubstring("unsupported format"));
}

TEST_CASE("FeatherDataParser - rejects .feather without .mbo.json prefix",
          "[FeatherDataParser]")
{
    TempFile tmp("bad.feather");
    {
        std::ofstream fs(tmp.getPath());
        fs << "not a valid feather file\n";
    }

    FeatherDataParser parser(tmp.getPath());
    REQUIRE_THROWS_WITH(parser.parse([](const MarketDataEvent&) {}),
                        Catch::Matchers::ContainsSubstring("unsupported format"));
}

TEST_CASE("FeatherDataParser - rejects .csv files", "[FeatherDataParser]")
{
    TempFile tmp("bad.csv");
    {
        std::ofstream fs(tmp.getPath());
        fs << "ts_recv,side,price\n";
    }

    FeatherDataParser parser(tmp.getPath());
    REQUIRE_THROWS_WITH(parser.parse([](const MarketDataEvent&) {}),
                        Catch::Matchers::ContainsSubstring("unsupported format"));
}
