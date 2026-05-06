// main function for the back-tester app
// please, keep it minimalistic

#include "common/BasicTypes.hpp"
#include "common/MarketDataEvent.hpp"
#include "main/MdEventConverter.hpp"
#include "main/PipelineRunner.hpp"

#include <iostream>
#include <string>

using namespace cmf;

namespace
{

void processMarketDataEvent(const MarketDataEvent &order)
{
    std::cout << "ts_recv_ns=" << order.tsRecv << " order_id=" << order.orderId
              << " side=" << MdEventConverter::toChar(order.side) << " price=";
    if (order.price.has_value())
    {
        std::cout << *order.price;
    }
    else
    {
        std::cout << "null";
    }
    std::cout << " size=" << order.size << " action=" << MdEventConverter::toChar(order.action) << std::endl;
}

void printResult(const PipelineRunner::PipelineResult &result)
{
    std::cout << "First 10 events:" << std::endl;
    for (const auto &firstEvent : result.firstTenEvents)
    {
        processMarketDataEvent(firstEvent);
    }

    std::cout << "Last 10 events:" << std::endl;
    for (const auto &lastEvent : result.lastTenEvents)
    {
        processMarketDataEvent(lastEvent);
    }

    std::cout << "Summary: total_messages_processed=" << result.totalMessagesProcessed << " first_timestamp_ns=";
    if (result.totalMessagesProcessed == 0)
    {
        std::cout << "n/a first_timestamp=n/a last_timestamp_ns=n/a last_timestamp=n/a";
    }
    else
    {
        char earliestIso[31];
        char latestIso[31];
        MdEventConverter::nanosToIsoTimestamp(*result.earliestTimestampNs, earliestIso);
        MdEventConverter::nanosToIsoTimestamp(*result.latestTimestampNs, latestIso);
        std::cout << *result.earliestTimestampNs
                  << " first_timestamp=" << earliestIso
                  << " last_timestamp_ns=" << *result.latestTimestampNs
                  << " last_timestamp=" << latestIso;
    }
    std::cout << std::endl;
}

} // namespace

int main(int argc, const char *argv[])
{
    try
    {
        if (argc != 2)
        {
            std::cerr << "Usage: back-tester <input_file_path>" << std::endl;
            return 1;
        }

        const std::string inputFilePath{argv[1]};
        PipelineRunner runner;
        const auto result = runner.run(inputFilePath);
        printResult(result);
    }
    catch (std::exception &ex)
    {
        std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
