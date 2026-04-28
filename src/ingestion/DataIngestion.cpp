#include "DataIngestion.hpp"
#include "ThreadSafeQueue.hpp"
#include "common/MarketDataParser.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

void ProcessMarketDataEvent(const MarketDataEvent& event)
{
    std::cout << event << "\n";
}

auto ListNDJSONFiles(const std::string& folder_path) -> std::vector<std::string>
{
    std::vector<std::string> files;
    try
    {
        for (const auto& entry : std::filesystem::directory_iterator(folder_path))
        {
            if (entry.is_regular_file())
            {
                const auto& path = entry.path();
                if (path.extension() == ".json" || path.extension() == ".ndjson")
                {
                    files.push_back(path.string());
                }
            }
        }
        std::sort(files.begin(), files.end());
    }
    catch (const std::filesystem::filesystem_error&)
    {
        // Silently ignore directory iteration errors
    }
    return files;
}

namespace
{

using Queue_t = ThreadSafeQueue<MarketDataEvent>;

static void ProducerThread(const std::string& file_path, Queue_t& queue)
{
    std::ifstream file{file_path};
    if (!file.is_open())
    {
        queue.MarkDone();
        return;
    }

    std::string line;
    while (std::getline(file, line))
    {
        auto event = parseNDJSON(line);
        if (event)
        {
            queue.Push(std::move(*event));
        }
    }
    queue.MarkDone();
}

struct HeapItem
{
    std::uint64_t ts_event;
    MarketDataEvent event;
    std::size_t queue_idx;

    bool operator>(const HeapItem& other) const
    {
        return ts_event > other.ts_event;
    }
};

} // namespace

void FlatMergerEngine::Ingest(
    const std::vector<std::string>& file_paths,
    EventCallback on_event)
{
    if (file_paths.empty())
        return;

    // Create queues
    std::vector<std::unique_ptr<Queue_t>> producer_queues;
    for (std::size_t i = 0; i < file_paths.size(); ++i)
    {
        producer_queues.push_back(std::make_unique<Queue_t>());
    }

    auto merged_queue = std::make_unique<Queue_t>();

    // Thread management
    std::vector<std::jthread> threads;

    // Start producer threads
    for (std::size_t i = 0; i < file_paths.size(); ++i)
    {
        threads.emplace_back(ProducerThread, file_paths[i], std::ref(*producer_queues[i]));
    }

    // Start merger thread (lambda captures producer_queues)
    threads.emplace_back([&producer_queues, &merged_queue]()
                         {
        std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap;

        // Initialize: take first event from each producer queue
        for (std::size_t i = 0; i < producer_queues.size(); ++i)
        {
            auto event = producer_queues[i]->BlockingPop();
            if (event)
            {
                heap.push(HeapItem{event->ts_event, std::move(*event), i});
            }
        }

        // Drain the heap: repeatedly pop minimum and fetch next from that queue
        while (!heap.empty())
        {
            auto [ts, event, idx] = heap.top();
            heap.pop();
            merged_queue->Push(std::move(event));

            auto next = producer_queues[idx]->BlockingPop();
            if (next)
            {
                heap.push(HeapItem{next->ts_event, std::move(*next), idx});
            }
        }

        merged_queue->MarkDone(); });

    // Start dispatcher thread (lambda captures merged_queue and on_event)
    threads.emplace_back([&merged_queue, on_event]()
                         {
        while (auto event = merged_queue->BlockingPop())
        {
            on_event(*event);
        } });

    // jthread destructors will join all threads in reverse order
    // Destruction order: dispatcher (joined first) → merger → producers
}

void HierarchyMergerEngine::Ingest(
    const std::vector<std::string>& file_paths,
    EventCallback on_event)
{
    if (file_paths.empty())
        return;

    std::vector<std::unique_ptr<Queue_t>> queues;
    std::vector<std::jthread> threads;

    // Create producer queues
    queues.reserve(file_paths.size() + file_paths.size()); // approximate reservation
    for (const auto& file_path : file_paths)
    {
        auto q = std::make_unique<Queue_t>();
        Queue_t* q_ptr = q.get();
        queues.push_back(std::move(q));
        threads.emplace_back(ProducerThread, file_path, std::ref(*q_ptr));
    }

    // Build merger tree layer by layer
    std::vector<std::size_t> current_level;
    for (std::size_t i = 0; i < file_paths.size(); ++i)
    {
        current_level.push_back(i);
    }

    while (current_level.size() > 1)
    {
        std::vector<std::size_t> next_level;

        // Pair up queues and create 2-way merger threads
        for (std::size_t i = 0; i + 1 < current_level.size(); i += 2)
        {
            std::size_t left_idx = current_level[i];
            std::size_t right_idx = current_level[i + 1];

            // Create output queue for this merger
            std::size_t out_idx = queues.size();
            queues.push_back(std::make_unique<Queue_t>());
            next_level.push_back(out_idx);

            // Start 2-way merger thread (lambda captures indices and queues)
            threads.emplace_back([&queues, left_idx, right_idx, out_idx]()
                                 {
                auto a = queues[left_idx]->BlockingPop();
                auto b = queues[right_idx]->BlockingPop();

                while (a || b)
                {
                    bool pick_left;
                    if (!a)
                    {
                        pick_left = false;
                    }
                    else if (!b)
                    {
                        pick_left = true;
                    }
                    else
                    {
                        pick_left = (a->ts_event <= b->ts_event);
                    }

                    if (pick_left)
                    {
                        queues[out_idx]->Push(std::move(*a));
                        a = queues[left_idx]->BlockingPop();
                    }
                    else
                    {
                        queues[out_idx]->Push(std::move(*b));
                        b = queues[right_idx]->BlockingPop();
                    }
                }

                queues[out_idx]->MarkDone(); });
        }

        // Handle odd queue (pass through to next level)
        if (current_level.size() % 2 == 1)
        {
            next_level.push_back(current_level.back());
        }

        current_level = std::move(next_level);
    }

    // The root queue is at current_level[0]
    std::size_t root_idx = current_level[0];

    // Start dispatcher thread (lambda captures root_idx, queues, and on_event)
    threads.emplace_back([&queues, root_idx, on_event]()
                         {
        while (auto event = queues[root_idx]->BlockingPop())
        {
            on_event(*event);
        } });

    // jthread destructors will join all threads in reverse order
}
