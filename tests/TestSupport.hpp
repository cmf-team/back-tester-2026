#pragma once

#include "app/ArgsParser.hpp"
#include "domain/MarketDataEvent.hpp"
#include "processing/IMarketDataEventProcessor.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace md::test {

inline constexpr std::uint64_t xeur_base_timestamp = 1773014400000000000ULL;

class CapturingProcessor final : public IMarketDataEventProcessor {
public:
    void processMarketDataEvent(const MarketDataEvent& event) override {
        events.push_back(event);
    }

    std::vector<MarketDataEvent> events;
};

inline void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline void requireContains(const std::string& text, std::string_view needle, const std::string& message) {
    require(text.find(needle) != std::string::npos, message);
}

inline std::filesystem::path makeTempDir(const std::string& name) {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto path = std::filesystem::temp_directory_path() / ("ingest_test_" + name + "_" + std::to_string(now));
    std::filesystem::create_directories(path);
    return path;
}

inline void writeFile(const std::filesystem::path& path, const std::string& content) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("cannot write test file: " + path.string());
    }
    file << content;
}

inline std::string nsFraction(std::uint64_t offset_ns) {
    std::string text = std::to_string(offset_ns);
    return std::string(9 - text.size(), '0') + text;
}

inline std::string line(
    std::uint64_t offset_ns,
    std::uint64_t order_id,
    char side = 'B',
    char action = 'A',
    const std::string& price_json = "\"1.085000000\""
) {
    const std::string timestamp = "2026-03-09T00:00:00." + nsFraction(offset_ns) + "Z";
    return "{\"ts_recv\":\"" + timestamp
        + "\",\"hd\":{\"ts_event\":\"" + timestamp
        + "\",\"rtype\":160,\"publisher_id\":101,\"instrument_id\":442}"
        + ",\"action\":\"" + std::string(1, action)
        + "\",\"side\":\"" + std::string(1, side)
        + "\",\"price\":" + price_json
        + ",\"size\":10,\"channel_id\":23,\"order_id\":\"" + std::to_string(order_id)
        + "\",\"flags\":0,\"ts_in_delta\":0,\"sequence\":0,\"symbol\":\"FCEU SI 20260316 PS\"}";
}

inline AppConfig parseArgs(std::vector<std::string> args) {
    std::vector<char*> argv;
    argv.reserve(args.size());
    for (auto& arg : args) {
        argv.push_back(arg.data());
    }
    return ArgsParser::parse(static_cast<int>(argv.size()), argv.data());
}

inline void expectArgsError(const std::vector<std::string>& args, const std::string& case_name) {
    try {
        parseArgs(args);
    } catch (const ArgsError&) {
        return;
    }

    throw std::runtime_error("expected ArgsError: " + case_name);
}

inline void requireChronological(const std::vector<MarketDataEvent>& events, const std::string& case_name) {
    for (std::size_t i = 1; i < events.size(); ++i) {
        require(!eventComesBefore(events[i], events[i - 1]), case_name + ": event order regression");
    }
}

inline void requireTimestampOffsets(
    const std::vector<MarketDataEvent>& events,
    const std::vector<std::uint64_t>& expected_offsets,
    const std::string& case_name
) {
    require(events.size() == expected_offsets.size(), case_name + ": unexpected event count");
    for (std::size_t i = 0; i < events.size(); ++i) {
        require(events[i].timestamp == xeur_base_timestamp + expected_offsets[i], case_name + ": unexpected timestamp order");
    }
}

inline void writeMultiFileDataset(const std::filesystem::path& dir) {
    writeFile(dir / "day1.ndjson", line(100, 1) + "\n" + line(400, 4) + "\n" + line(700, 7) + "\n");
    writeFile(dir / "day2.ndjson", line(200, 2) + "\n" + line(500, 5) + "\n" + line(800, 8) + "\n");
    writeFile(dir / "nested" / "day3.ndjson", line(300, 3) + "\n" + line(600, 6) + "\n" + line(900, 9) + "\n");
}

inline std::filesystem::path testDataDir() {
#ifdef TEST_DATA_DIR
    return TEST_DATA_DIR;
#else
    return std::filesystem::current_path() / "tests" / "test_data";
#endif
}

} // namespace md::test
