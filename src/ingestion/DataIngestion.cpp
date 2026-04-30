#include "DataIngestion.hpp"
#include "ThreadSafeQueue.hpp"
#include "common/MarketDataParser.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <queue>
#include <thread>
#include <vector>

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

using queue_t = ThreadSafeQueue<MarketDataEvent>;
constexpr std::size_t BATCH_SIZE = 1024;

static void ProducerThread(const std::string& file_path, queue_t& queue)
{
    std::ifstream file{file_path};
    if (!file.is_open())
    {
        queue.MarkDone();
        return;
    }

    std::vector<MarketDataEvent> batch;
    batch.reserve(BATCH_SIZE);

    std::string line;
    while (std::getline(file, line))
    {
        auto event = parseNDJSON(line);
        if (event)
        {
            batch.push_back(std::move(*event));
            if (batch.size() >= BATCH_SIZE)
            {
                queue.PushBatch(batch);
                batch.clear();
            }
        }
    }

    if (!batch.empty())
    {
        queue.PushBatch(batch);
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
    std::vector<std::unique_ptr<queue_t>> producer_queues;
    for (std::size_t i = 0; i < file_paths.size(); ++i)
    {
        producer_queues.push_back(std::make_unique<queue_t>());
    }

    auto merged_queue = std::make_unique<queue_t>();

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
        std::vector<std::optional<MarketDataEvent>> pending(producer_queues.size());

        // Initialize: pop first event from each queue
        for (std::size_t i = 0; i < producer_queues.size(); ++i)
        {
            pending[i] = producer_queues[i]->BlockingPop();
            if (pending[i])
            {
                heap.push(HeapItem{pending[i]->ts_event, std::move(*pending[i]), i});
                pending[i].reset();
            }
        }

        std::vector<MarketDataEvent> merged_batch;
        merged_batch.reserve(BATCH_SIZE);

        while (!heap.empty())
        {
            auto [ts, event, idx] = heap.top();
            heap.pop();
            merged_batch.push_back(std::move(event));

            if (merged_batch.size() >= BATCH_SIZE)
            {
                merged_queue->PushBatch(merged_batch);
                merged_batch.clear();
            }

            // Get next from same queue
            auto next = producer_queues[idx]->BlockingPop();
            if (next)
            {
                heap.push(HeapItem{next->ts_event, std::move(*next), idx});
            }
        }

        if (!merged_batch.empty())
        {
            merged_queue->PushBatch(merged_batch);
        }

        merged_queue->MarkDone(); });

    // Start dispatcher thread (lambda captures merged_queue and on_event)
    threads.emplace_back([&merged_queue, on_event]()
                         {
        std::vector<MarketDataEvent> batch;
        while (merged_queue->PopBatch(batch, BATCH_SIZE) > 0)
        {
            for (const auto& event : batch)
            {
                on_event(event);
            }
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

    std::vector<std::unique_ptr<queue_t>> queues;
    std::vector<std::jthread> threads;

    // Create producer queues
    queues.reserve(file_paths.size() + file_paths.size()); // approximate reservation
    for (const auto& file_path : file_paths)
    {
        auto q = std::make_unique<queue_t>();
        queue_t* q_ptr = q.get();
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
            queues.push_back(std::make_unique<queue_t>());
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
        std::vector<MarketDataEvent> batch;
        while (queues[root_idx]->PopBatch(batch, BATCH_SIZE) > 0)
        {
            for (const auto& event : batch)
            {
                on_event(event);
            }
        } });

    // jthread destructors will join all threads in reverse order
}
