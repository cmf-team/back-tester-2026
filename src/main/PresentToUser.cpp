#include "PresentToUser.hpp"
#include "../common/MarketDataParser.hpp"
#include <iomanip>

void PresentToUser(std::ostream& out, std::istream& file)
{
    ParseSummary summary;
    std::deque<MarketDataEvent> events;

    const auto t0 = std::chrono::steady_clock::now();

    out << "\n--- Objective 1 Verification ---\n";

    std::string line;
    while (std::getline(file, line))
    {
        if (line.empty())
            continue;

        auto ev = parseNDJSON(line);
        if (!ev)
            continue;

        events.push_back(*ev);
        if (events.size() == 10)
        {
            out << "\nFirst " << events.size() << " Events:\n";
            for (const auto& e : events)
                out << e << "\n";
            out << "\nProcessing rest of the messages: ...\n";
        }
        if (events.size() > 10)
            events.pop_front();

        ++summary.total_events;
    }

    summary.processing_time_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - t0)
            .count());

    out << "\nLast " << events.size() << " Events:\n";
    for (const auto& e : events)
        out << e << "\n";

    out << "\nSummary:\n"
        << "  Total Messages:   " << summary.total_events << "\n"
        << "  Processing Time:  " << summary.processing_time_ns << " ns\n"
        << "  Processing Time:  " << std::setprecision(3) << summary.processing_time_ns / 1'000'000 << " ms\n";
}
