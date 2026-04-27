#include "MarketData.hpp"
#include "OrderBook.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

struct ParseError : std::exception {
    const char *what() const noexcept override { return "Parse error"; }
};

std::string_view trim_quotes(std::string_view sv) {
    if (sv.size() >= 2 && sv.front() == '"' && sv.back() == '"') {
        return sv.substr(1, sv.size() - 2);
    }
    return sv;
}

template <typename T>
bool parse_number(std::string_view sv, T &out) {
    auto *begin = sv.data();
    auto *end = sv.data() + sv.size();
    auto res = std::from_chars(begin, end, out);
    return res.ec == std::errc{} && res.ptr == end;
}

std::string_view extract_field(std::string_view line, std::string_view key) {
    auto pos = line.find(key);
    if (pos == std::string_view::npos) return {};

    pos = line.find(':', pos);
    if (pos == std::string_view::npos) return {};

    auto start = pos + 1;
    auto end = line.find_first_of(",}", start);
    if (end == std::string_view::npos) end = line.size();

    while (start < end && line[start] == ' ') ++start;
    while (end > start && line[end - 1] == ' ') --end;

    return line.substr(start, end - start);
}

bool parse_ts_event(std::string_view sv, cmf::NanoTime &out) {
    sv = trim_quotes(sv);
    if (sv.size() != 30) return false;

    if (sv[4] != '-' || sv[7] != '-' || sv[10] != 'T' ||
        sv[13] != ':' || sv[16] != ':' || sv[19] != '.' || sv[29] != 'Z') {
        return false;
    }

    int y = 0, m = 0, d = 0;
    int hour = 0, minute = 0, second = 0;
    int nanos = 0;

    if (!parse_number(sv.substr(0, 4), y)) return false;
    if (!parse_number(sv.substr(5, 2), m)) return false;
    if (!parse_number(sv.substr(8, 2), d)) return false;
    if (!parse_number(sv.substr(11, 2), hour)) return false;
    if (!parse_number(sv.substr(14, 2), minute)) return false;
    if (!parse_number(sv.substr(17, 2), second)) return false;
    if (!parse_number(sv.substr(20, 9), nanos)) return false;

    using namespace std::chrono;

    const year_month_day ymd{
        year{y} / month{static_cast<unsigned>(m)} / day{static_cast<unsigned>(d)}
    };

    if (!ymd.ok()) return false;

    const sys_days days{ymd};
    const auto tp = time_point_cast<nanoseconds>(days)
                  + hours{hour}
                  + minutes{minute}
                  + seconds{second}
                  + nanoseconds{nanos};

    out = static_cast<cmf::NanoTime>(tp.time_since_epoch().count());
    return true;
}

MarketDataEvent parse_event(std::string_view line) {
    MarketDataEvent e{};

    auto ts_sv = extract_field(line, "\"ts_event\"");
    auto instr_sv = extract_field(line, "\"instrument_id\"");

    auto order_id_sv = trim_quotes(extract_field(line, "\"order_id\""));
    auto action_sv = trim_quotes(extract_field(line, "\"action\""));
    auto side_sv = trim_quotes(extract_field(line, "\"side\""));
    auto price_sv = trim_quotes(extract_field(line, "\"price\""));
    auto size_sv = extract_field(line, "\"size\"");

    if (!parse_ts_event(ts_sv, e.ts_event)) throw ParseError{};
    if (!parse_number(instr_sv, e.instrument_id)) throw ParseError{};
    if (!parse_number(order_id_sv, e.order_id)) throw ParseError{};

    if (side_sv == "B") {
        e.side = cmf::Side::Buy;
    } else if (side_sv == "A") {
        e.side = cmf::Side::Sell;
    } else if (side_sv == "N") {
        e.side = cmf::Side::None;
    } else {
        throw ParseError{};
    }

    if (price_sv == "null") {
        e.price = 0;
    } else {
        if (!parse_number(price_sv, e.price)) throw ParseError{};
    }

    if (!parse_number(size_sv, e.size)) throw ParseError{};

    if (action_sv == "A") {
        e.action = OrderAction::Add;
    } else if (action_sv == "R") {
        if (e.order_id == 0 && e.price == 0 && e.size == 0) {
            e.action = OrderAction::Clear;
        } else {
            e.action = OrderAction::Cancel;
        }
    } else if (action_sv == "M") {
        e.action = OrderAction::Modify;
    } else if (action_sv == "T") {
        e.action = OrderAction::Trade;
    } else if (action_sv == "F") {
        e.action = OrderAction::Fill;
    } else {
        throw ParseError{};
    }

    return e;
}

bool is_valid_event(const MarketDataEvent &e) {
    if (e.instrument_id == 0) return false;
    if (e.action == OrderAction::None) return false;
    if (e.action == OrderAction::Clear) return true;
    if (e.order_id == 0) return false;
    if (e.action == OrderAction::Add && (e.price <= 0 || e.size <= 0)) return false;
    return true;
}

struct StreamState {
    fs::path path;
    std::ifstream file;
    std::size_t file_index{};
    std::size_t line_no{};
    bool eof{false};
};

struct HeapItem {
    MarketDataEvent event;
    std::size_t file_index{};
    std::size_t line_no{};
};

struct HeapCompare {
    bool operator()(const HeapItem &a, const HeapItem &b) const {
        if (a.event.ts_event != b.event.ts_event) {
            return a.event.ts_event > b.event.ts_event;
        }
        if (a.file_index != b.file_index) {
            return a.file_index > b.file_index;
        }
        return a.line_no > b.line_no;
    }
};

static std::vector<fs::path> collect_json_files(const fs::path &folder) {
    std::vector<fs::path> files;

    for (const auto &entry : fs::directory_iterator(folder)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;
        files.push_back(entry.path());
    }

    std::sort(files.begin(), files.end());
    return files;
}

static bool load_next_valid_event(StreamState &st, HeapItem &out) {
    if (st.eof || !st.file.is_open()) return false;

    std::string line;
    while (std::getline(st.file, line)) {
        ++st.line_no;

        if (line.empty()) continue;

        try {
            MarketDataEvent e = parse_event(line);
            if (!is_valid_event(e)) continue;

            out.event = e;
            out.file_index = st.file_index;
            out.line_no = st.line_no;
            return true;
        } catch (const ParseError &) {
            continue;
        }
    }

    st.eof = true;
    return false;
}

static void print_report_for_book(const InstrumentId instrument_id, const LimitOrderBook &book) {
    std::cout << "\nINSTRUMENT " << instrument_id << "\n";
    book.print_snapshot(3);
    std::cout << "best bid: " << book.best_bid() << "\n";
    std::cout << "best ask: " << book.best_ask() << "\n";
}

int main(int argc, const char *argv[]) {
    try {
        if (argc < 2) {
            std::cerr << "DIRECTORY NOT PROVIDED\n";
            return 1;
        }

        fs::path folder = argv[1];
        if (!fs::exists(folder) || !fs::is_directory(folder)) {
            std::cerr << "INVALID DIRECTORY\n";
            return 1;
        }

        auto files = collect_json_files(folder);
        if (files.empty()) {
            std::cerr << "NO JSON FILES FOUND\n";
            return 1;
        }

        std::vector<StreamState> streams;
        streams.reserve(files.size());

        for (std::size_t i = 0; i < files.size(); ++i) {
            StreamState st;
            st.path = files[i];
            st.file_index = i;
            st.file.open(st.path);
            if (!st.file.is_open()) {
                std::cerr << "FAILED TO OPEN FILE: " << st.path.string() << '\n';
                continue;
            }
            streams.push_back(std::move(st));
        }

        if (streams.empty()) {
            std::cerr << "NO OPENABLE FILES\n";
            return 1;
        }

        std::priority_queue<HeapItem, std::vector<HeapItem>, HeapCompare> heap;

        for (auto &st : streams) {
            HeapItem item;
            if (load_next_valid_event(st, item)) {
                heap.push(std::move(item));
            }
        }

        std::unordered_map<InstrumentId, LimitOrderBook> books;
        std::size_t total_events = 0;
        std::size_t snapshots_taken = 0;

        auto start = std::chrono::high_resolution_clock::now();

        while (!heap.empty()) {
            HeapItem item = heap.top();
            heap.pop();

            const auto &e = item.event;

            books[e.instrument_id].apply(e);
            ++total_events;

            if (total_events == 1 || total_events % 1000000 == 0) {
                std::cout << "\nSNAPSHOT after " << total_events << " events\n";
                print_report_for_book(e.instrument_id, books[e.instrument_id]);
                ++snapshots_taken;
            }

            HeapItem next_item;
            if (load_next_valid_event(streams[item.file_index], next_item)) {
                heap.push(std::move(next_item));
            }
        }

        auto finish = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration<double>(finish - start).count();

        std::cout << "\nSUMMARY\n";
        std::cout << "total events: " << total_events << "\n";
        std::cout << "snapshots taken: " << snapshots_taken << "\n";
        std::cout << "processing time: " << sec << " sec\n";
        std::cout << "events/sec: " << (sec > 0 ? total_events / sec : 0.0) << "\n";

        std::size_t printed = 0;
        for (const auto &[instrument_id, book] : books) {
            if (printed++ == 3) break;
            print_report_for_book(instrument_id, book);
        }

    } catch (const std::exception &ex) {
        std::cerr << "Back-tester threw an exception: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}