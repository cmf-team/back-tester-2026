// main function for the back-tester app
// please, keep it minimalistic

#include "PresentToUser.hpp"
#include "common/MarketDataEvent.hpp"
#include "ingestion/DataIngestion.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, const char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: back-tester <path_to_folder_or_file>\n";
        return 1;
    }

    std::string path{argv[1]};
    std::error_code ec;
    auto status = std::filesystem::status(path, ec);

    if (!std::filesystem::exists(status))
    {
        std::cerr << "Error: path does not exist: " << path << "\n";
        return 1;
    }

    if (std::filesystem::is_regular_file(status))
    {
        // Single file: use PresentToUser (original behavior)
        std::ifstream file{path};
        if (!file.is_open())
        {
            std::cerr << "Error: could not open " << path << "\n";
            return 1;
        }
        PresentToUser(std::cout, file);
    }
    else if (std::filesystem::is_directory(status))
    {
        // Folder: use FlatMergerEngine to process all NDJSON files
        auto files = ListNDJSONFiles(path);
        if (files.empty())
        {
            std::cerr << "Error: no NDJSON files found in " << path << "\n";
            return 1;
        }
        FlatMergerEngine engine;
        std::vector<MarketDataEvent> events;
        const auto t0 = std::chrono::steady_clock::now();
        // engine.Ingest(files, ProcessMarketDataEvent);
        engine.Ingest(files, [&](const MarketDataEvent& e)
                      { events.push_back(e); });
        auto processing_time_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - t0)
                .count());
        std::cout << "\nProcessed " << files.size() << " files in " << processing_time_ns << " ns ("
                  << std::setprecision(3) << processing_time_ns / 1'000'000 << " ms)\n";
        std::cout << "Total events processed: " << events.size() << "\n";
        // Print first and last 10 events as a sample
        for (size_t i = 0; i < std::min(events.size(), static_cast<size_t>(10)); ++i)
        {
            std::cout << events[i] << "\n";
        }
        if (events.size() > 20)
        {
            std::cout << "...\n";
            for (size_t i = events.size() - 10; i < events.size(); ++i)
            {
                std::cout << events[i] << "\n";
            }
        }
    }
    else
    {
        std::cerr << "Error: path is neither a file nor a directory: " << path << "\n";
        return 1;
    }

    return 0;
}
