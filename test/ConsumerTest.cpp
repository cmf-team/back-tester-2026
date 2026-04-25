// Integration test for Consumer::runStandardTask on a small NDJSON file.

#include "market_data/Consumer.hpp"

#include "TempFile.hpp"
#include "catch2/catch_all.hpp"

#include <fstream>
#include <sstream>
#include <string>

using namespace cmf;

TEST_CASE("runStandardTask - processes 3-line NDJSON and returns summary",
          "[Consumer]")
{
    TempFile tf("consumer_smoke.ndjson");
    {
        std::ofstream out(tf.getPath(), std::ios::binary);
        REQUIRE(out.is_open());
        // Three real lines drawn from XEUR-20260409-HTT6HHLT6R (Clear, Add,
        // Cancel). Kept terse and escaped here to avoid the raw-string pitfall
        // of embedding a "})" sequence.
        out << R"({"ts_recv":"2026-04-06T18:53:08.486368500Z","hd":{"ts_event":"2026-04-06T18:53:08.486361336Z","rtype":160,"publisher_id":101,"instrument_id":453},"action":"R","side":"N","price":null,"size":0,"channel_id":23,"order_id":"0","flags":128,"ts_in_delta":7164,"sequence":0,"symbol":"FCEU SI 20281218 PS"})"
            << "\n"
            << R"({"ts_recv":"2026-04-07T00:00:00.246103535Z","hd":{"ts_event":"2026-04-07T00:00:00.246086711Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"A","side":"B","price":"1.157500000","size":20,"channel_id":23,"order_id":"10998892037100869125","flags":128,"ts_in_delta":985,"sequence":81255,"symbol":"FCEU SI 20260615 PS"})"
            << "\n"
            << R"({"ts_recv":"2026-04-07T00:00:02.399364973Z","hd":{"ts_event":"2026-04-07T00:00:02.399352523Z","rtype":160,"publisher_id":101,"instrument_id":436},"action":"C","side":"B","price":"1.157500000","size":20,"channel_id":23,"order_id":"10998892037100869125","flags":128,"ts_in_delta":882,"sequence":81351,"symbol":"FCEU SI 20260615 PS"})"
            << "\n";
    }

    std::ostringstream buf;
    const auto s =
        runStandardTask(tf.getPath(), /*head_n=*/10, /*tail_n=*/10, buf);

    REQUIRE(s.total == 3);
    REQUIRE(s.first_ts == 1775501588486368500LL);
    REQUIRE(s.last_ts == 1775520002399364973LL);

    const std::string out = buf.str();
    REQUIRE(out.find("Total messages: 3") != std::string::npos);
    REQUIRE(out.find("=== First 3 events ===") != std::string::npos);
    REQUIRE(out.find("=== Last 3 events ===") != std::string::npos);
    REQUIRE(out.find("action=R") != std::string::npos);
    REQUIRE(out.find("action=A") != std::string::npos);
    REQUIRE(out.find("action=C") != std::string::npos);
}

TEST_CASE("runStandardTask - empty file yields zero total", "[Consumer]")
{
    TempFile tf("consumer_empty.ndjson");
    {
        std::ofstream out(tf.getPath(), std::ios::binary);
    }

    std::ostringstream buf;
    const auto s = runStandardTask(tf.getPath(), 10, 10, buf);
    REQUIRE(s.total == 0);
    REQUIRE(s.first_ts == 0);
    REQUIRE(s.last_ts == 0);
}
