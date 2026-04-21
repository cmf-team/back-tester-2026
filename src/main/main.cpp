#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include <fstream>
#include <memory>
#include <condition_variable>
// #include "simdjson.h"
#include <filesystem>
#include <time.h>

// -----------------------------------------------------------------------------
// Core Data Structures & Constants
// -----------------------------------------------------------------------------

// Fixed precision for prices (1e-9)[cite: 117].
constexpr int64_t PRICE_MULTIPLIER = 1000000000;

// Align to typical cache line size (64 bytes) to prevent false sharing.
struct alignas(64) MarketDataEvent {
    uint64_t ts_event;  // Index timestamp (ts_recv or ts_event) [cite: 67-68]
    uint64_t order_id;
    int64_t price;      
    uint32_t size;
    char side;          // 'A', 'B', or 'N' [cite: 129-140]
    char action;        // 'A', 'M', 'C', 'R', 'T', 'F', 'N' [cite: 153]
    bool is_valid;      // Used for stream termination signals
};

// -----------------------------------------------------------------------------
// Lock-Free Single-Producer Single-Consumer (SPSC) Queue
// -----------------------------------------------------------------------------
template <typename T, size_t Capacity>
class SPSCQueue {
public:
    SPSCQueue() : head_(0), tail_(0) {}

    bool push(const T& item) {
        const size_t current_tail = tail_.load(std::memory_order_relaxed);
        const size_t next_tail = (current_tail + 1) % Capacity;
        if (next_tail == head_.load(std::memory_order_acquire)) {
            return false; // Queue is full
        }
        buffer_[current_tail] = item;
        tail_.store(next_tail, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        const size_t current_head = head_.load(std::memory_order_relaxed);
        if (current_head == tail_.load(std::memory_order_acquire)) {
            return false; // Queue is empty
        }
        item = buffer_[current_head];
        head_.store((current_head + 1) % Capacity, std::memory_order_release);
        return true;
    }

private:
    T buffer_[Capacity];
    alignas(64) std::atomic<size_t> head_;
    alignas(64) std::atomic<size_t> tail_;
};

using EventQueue = SPSCQueue<MarketDataEvent, 65536>;

// -----------------------------------------------------------------------------
// Producer: File Reader & Parser
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Producer: File Reader & Parser
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Type-Safe Helpers for Financial JSON Parsing
// -----------------------------------------------------------------------------
bool extract_uint64(simdjson::dom::object obj, std::string_view key, uint64_t& out_val) {
    simdjson::dom::element elem;
    if (obj[key].get(elem) == simdjson::SUCCESS) {
        if (elem.type() == simdjson::dom::element_type::UINT64) { 
            out_val = uint64_t(elem); return true; 
        }
        if (elem.type() == simdjson::dom::element_type::INT64) { 
            out_val = static_cast<uint64_t>(int64_t(elem)); return true; 
        }
        if (elem.type() == simdjson::dom::element_type::STRING) { 
            out_val = std::stoull(std::string(elem.get_string().value())); return true; 
        }
    }
    return false;
}

bool extract_int64(simdjson::dom::object obj, std::string_view key, int64_t& out_val) {
    simdjson::dom::element elem;
    if (obj[key].get(elem) == simdjson::SUCCESS) {
        if (elem.is_null()) { 
            out_val = 9223372036854775807LL; return true; // UNDEF_PRICE handling 
        }
        if (elem.type() == simdjson::dom::element_type::INT64) { 
            out_val = int64_t(elem); return true; 
        }
        if (elem.type() == simdjson::dom::element_type::UINT64) { 
            out_val = static_cast<int64_t>(uint64_t(elem)); return true; 
        }
        if (elem.type() == simdjson::dom::element_type::STRING) { 
            out_val = std::stoll(std::string(elem.get_string().value())); return true; 
        }
    }
    return false;
}
inline uint64_t parse_ts(const std::string& s) {
    if (s.empty()) return 0;
    // Если это сырые наносекунды (Unix Epoch), парсим напрямую
    if (s.find('T') == std::string::npos) return std::stoull(s); 
    
    std::tm tm{};
    // Быстрый ручной парсинг "YYYY-MM-DDTHH:MM:SS"
    sscanf(s.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", 
           &tm.tm_year, &tm.tm_mon, &tm.tm_mday, 
           &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
    
    tm.tm_year -= 1900;
    tm.tm_mon -= 1;
    
    uint64_t epoch_sec = timegm(&tm); 
    uint64_t nanos = 0;
    
    // Вытягиваем наносекунды из дробной части (после точки)
    auto dot_pos = s.find('.');
    if (dot_pos != std::string::npos) {
        std::string frac = s.substr(dot_pos + 1);
        if (!frac.empty() && frac.back() == 'Z') frac.pop_back();
        while(frac.length() < 9) frac += "0"; // Дополняем до 9 нулей
        if (frac.length() > 9) frac = frac.substr(0, 9);
        nanos = std::stoull(frac);
    }
    return epoch_sec * 1000000000ULL + nanos;
}

bool extract_ts(simdjson::dom::object obj, std::string_view key, uint64_t& out_val) {
    simdjson::dom::element elem;
    if (obj[key].get(elem) == simdjson::SUCCESS) {
        if (elem.type() == simdjson::dom::element_type::UINT64) { 
            out_val = uint64_t(elem); return true; 
        }
        if (elem.type() == simdjson::dom::element_type::STRING) { 
            out_val = parse_ts(std::string(elem.get_string().value())); return true; 
        }
    }
    return false;
}

bool extract_price(simdjson::dom::object obj, std::string_view key, int64_t& out_val) {
    simdjson::dom::element elem;
    if (obj[key].get(elem) == simdjson::SUCCESS) {
        if (elem.is_null()) { 
            out_val = 9223372036854775807LL; return true; // UNDEF_PRICE
        }
        if (elem.type() == simdjson::dom::element_type::DOUBLE) {
            // Если пришел float: умножаем на 1e9
            out_val = static_cast<int64_t>(double(elem) * PRICE_MULTIPLIER); return true;
        }
        if (elem.type() == simdjson::dom::element_type::STRING) { 
            // Если пришла строка с десятичной дробью
            std::string s(elem.get_string().value());
            out_val = static_cast<int64_t>(std::stod(s) * PRICE_MULTIPLIER); return true;
        }
        if (elem.type() == simdjson::dom::element_type::INT64) {
            out_val = int64_t(elem); return true;
        }
    }
    return false;
}

// -----------------------------------------------------------------------------
// Producer: File Reader & Parser
// -----------------------------------------------------------------------------
void file_producer_loop(const std::string& filepath, std::shared_ptr<EventQueue> out_queue) {
    auto send_poison_pill = [&out_queue]() {
        MarketDataEvent poison_pill{};
        poison_pill.is_valid = false;
        while (!out_queue->push(poison_pill)) { std::this_thread::yield(); }
    };

    simdjson::dom::parser parser;
    simdjson::dom::document_stream docs;
    
    if (parser.load_many(filepath).get(docs)) {
        std::cerr << "Failed to load or parse " << filepath << "\n";
        send_poison_pill();
        return;
    }

    bool first_error = true;

    for (simdjson::dom::element doc_elem : docs) {
        simdjson::dom::object doc;
        if (doc_elem.get(doc) != simdjson::SUCCESS) continue;

        MarketDataEvent event{};
        
        // 1. Timestamp (Fallback from ts_recv to ts_event)
        if (!extract_ts(doc, "ts_recv", event.ts_event)) {
            if (!extract_ts(doc, "ts_event", event.ts_event)) {
                continue; 
            }
        }

        // 2. Order ID
        if (!extract_uint64(doc, "order_id", event.order_id)) event.order_id = 0;

        // 3. Price
        if (!extract_price(doc, "price", event.price)) {
            event.price = 9223372036854775807LL;
        }
        uint64_t sz;
        if (extract_uint64(doc, "size", sz)) event.size = static_cast<uint32_t>(sz);
        else event.size = 0;

        // 4. Side & Action
        simdjson::dom::element str_elem;
        if (doc["side"].get(str_elem) == simdjson::SUCCESS && str_elem.type() == simdjson::dom::element_type::STRING) {
            std::string_view s = str_elem.get_string().value();
            event.side = s.empty() ? 'N' : s[0];
        } else {
            event.side = 'N';
        }

        if (doc["action"].get(str_elem) == simdjson::SUCCESS && str_elem.type() == simdjson::dom::element_type::STRING) {
            std::string_view a = str_elem.get_string().value();
            event.action = a.empty() ? 'N' : a[0];
        } else {
            event.action = 'N';
        }

        event.is_valid = true;

        // 5. Push to Queue
        while (!out_queue->push(event)) { 
            std::this_thread::yield(); 
        }
    }

    send_poison_pill();
}
// -----------------------------------------------------------------------------
// Strategy 1: Flat Merger (K-Way Merge)
// -----------------------------------------------------------------------------
struct QueuePointer {
    MarketDataEvent current_event;
    size_t queue_index;

    bool operator>(const QueuePointer& other) const {
        return current_event.ts_event > other.current_event.ts_event;
    }
};

void flat_merger_loop(const std::vector<std::shared_ptr<EventQueue>>& input_queues, 
                      std::shared_ptr<EventQueue> output_queue) {
    
    std::priority_queue<QueuePointer, std::vector<QueuePointer>, std::greater<QueuePointer>> pq;
    size_t active_queues = input_queues.size();

    // Initial population
    for (size_t i = 0; i < input_queues.size(); ++i) {
        MarketDataEvent event;
        while (!input_queues[i]->pop(event)) { std::this_thread::yield(); }
        if (event.is_valid) {
            pq.push({event, i});
        } else {
            active_queues--;
        }
    }

    // K-way merge process
    while (active_queues > 0) {
        auto top = pq.top();
        pq.pop();

        while (!output_queue->push(top.current_event)) { std::this_thread::yield(); }

        MarketDataEvent next_event;
        while (!input_queues[top.queue_index]->pop(next_event)) { std::this_thread::yield(); }
        
        if (next_event.is_valid) {
            pq.push({next_event, top.queue_index});
        } else {
            active_queues--;
        }
    }

    MarketDataEvent poison_pill{};
    poison_pill.is_valid = false;
    while (!output_queue->push(poison_pill)) { std::this_thread::yield(); }
}

// -----------------------------------------------------------------------------
// Strategy 2: Hierarchy Merger (Tree Merge) - Pairwise node
// -----------------------------------------------------------------------------
void pairwise_merger_loop(std::shared_ptr<EventQueue> q1, 
                          std::shared_ptr<EventQueue> q2, 
                          std::shared_ptr<EventQueue> out_q) {
    MarketDataEvent e1, e2;
    bool q1_active = true, q2_active = true;

    auto fetch = [](std::shared_ptr<EventQueue>& q, MarketDataEvent& e, bool& active) {
        if (!active) return;
        while (!q->pop(e)) { std::this_thread::yield(); }
        if (!e.is_valid) active = false;
    };

    fetch(q1, e1, q1_active);
    fetch(q2, e2, q2_active);

    while (q1_active || q2_active) {
        if (q1_active && (!q2_active || e1.ts_event <= e2.ts_event)) {
            while (!out_q->push(e1)) { std::this_thread::yield(); }
            fetch(q1, e1, q1_active);
        } else if (q2_active) {
            while (!out_q->push(e2)) { std::this_thread::yield(); }
            fetch(q2, e2, q2_active);
        }
    }

    MarketDataEvent poison_pill{};
    poison_pill.is_valid = false;
    while (!out_q->push(poison_pill)) { std::this_thread::yield(); }
}

// -----------------------------------------------------------------------------
// Dispatcher
// -----------------------------------------------------------------------------
void processMarketDataEvent(const MarketDataEvent& event) {
    // Logic for updating LOB or feeding downstream models (e.g., Greeks, BS)
    // std::cout << "TS: " << event.ts_event << " Action: " << event.action << "\n";
}

void dispatcher_loop(std::shared_ptr<EventQueue> merged_queue, uint64_t& total_processed) {
    MarketDataEvent event;
    total_processed = 0;

    std::vector<MarketDataEvent> first_10;
    first_10.reserve(10);
    
    MarketDataEvent last_10[10];
    size_t last_10_idx = 0;

    while (true) {
        if (merged_queue->pop(event)) {
            if (!event.is_valid) break;
            
            if (total_processed < 10) {
                first_10.push_back(event);
            }
            
            last_10[last_10_idx] = event;
            last_10_idx = (last_10_idx + 1) % 10;
            
            processMarketDataEvent(event);
            total_processed++;
        } else {
            std::this_thread::yield();
        }
    }

    // Вывод результатов по требованию задания
    std::cout << "\n--- First 10 Events ---\n";
    for (const auto& e : first_10) {
        std::cout << "TS: " << e.ts_event << " Action: " << e.action << " Price: " << e.price << "\n";
    }

    std::cout << "\n--- Last 10 Events ---\n";
    size_t count_to_print = std::min(total_processed, static_cast<uint64_t>(10));
    size_t start_idx = (total_processed < 10) ? 0 : last_10_idx;
    for (size_t i = 0; i < count_to_print; ++i) {
        const auto& e = last_10[(start_idx + i) % 10];
        std::cout << "TS: " << e.ts_event << " Action: " << e.action << " Price: " << e.price << "\n";
    }
}

// -----------------------------------------------------------------------------
// Orchestration & Benchmark
// -----------------------------------------------------------------------------
void run_benchmark(const std::string& directory_path, bool use_flat_merger) {
    std::vector<std::shared_ptr<EventQueue>> producer_queues;
    std::vector<std::thread> threads;

    // Чтение файлов из переданной директории
    for (const auto& entry : std::filesystem::directory_iterator(directory_path)) {
        std::string filename = entry.path().filename().string();
        
        // Берем ТОЛЬКО файлы с рыночными данными, игнорируем manifest, metadata и т.д.
        if (filename.find(".mbo.json") != std::string::npos) {
            producer_queues.push_back(std::make_shared<EventQueue>());
            threads.emplace_back(file_producer_loop, entry.path().string(), producer_queues.back());
        }
    }

    if (producer_queues.empty()) {
        std::cerr << "No JSON files found in " << directory_path << "\n";
        return;
    }

    auto final_queue = std::make_shared<EventQueue>();
    auto start_time = std::chrono::high_resolution_clock::now();

if (use_flat_merger) {
        threads.emplace_back(flat_merger_loop, producer_queues, final_queue);
    } else {
        std::vector<std::shared_ptr<EventQueue>> current_level = producer_queues;
        while (current_level.size() > 1) {
            std::vector<std::shared_ptr<EventQueue>> next_level;
            for (size_t i = 0; i < current_level.size(); i += 2) {
                if (i + 1 < current_level.size()) {
                    auto out_q = (current_level.size() == 2) ? final_queue : std::make_shared<EventQueue>();
                    next_level.push_back(out_q);
                    threads.emplace_back(pairwise_merger_loop, current_level[i], current_level[i+1], out_q);
                } else {
                    next_level.push_back(current_level[i]);
                }
            }
            current_level = next_level;
        }
        // Если изначально был только один файл
        if (current_level.size() == 1 && current_level[0] != final_queue) {
            threads.emplace_back(pairwise_merger_loop, current_level[0], std::make_shared<EventQueue>(), final_queue);
        }
    }

    uint64_t total_processed = 0;
    std::thread dispatcher(dispatcher_loop, final_queue, std::ref(total_processed));

    for (auto& t : threads) {
        if (t.joinable()) t.join();
    }
    dispatcher.join();

    auto end_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> wall_clock = end_time - start_time;

    std::cout << "Directory: " << directory_path << "\n";
    std::cout << "Strategy: " << (use_flat_merger ? "Flat" : "Hierarchy") << "\n";
    std::cout << "Total processed: " << total_processed << "\n";
    std::cout << "Wall-clock time: " << wall_clock.count() << " s\n";
    std::cout << "Throughput: " << (total_processed / wall_clock.count()) << " msg/s\n\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_data_folder_1> [path_to_data_folder_2]\n";
        return 1;
    }

    // Запуск бенчмарка для каждой папки, переданной в аргументах
    for (int i = 1; i < argc; ++i) {
        run_benchmark(argv[i], true); 
    }
    
    return 0;
}