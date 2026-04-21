#include "common/BasicTypes.hpp"

#include <charconv>
#include <chrono>
#include <deque>
#include <exception>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

enum class OrderAction : signed char {
    None = 0,
    Add = 1,
    Modify = 2,
    Cancel = 3,
    Trade = 4,
    Fill = 5,
    Clear = 6
};

struct MarketDataEvent {
    cmf::NanoTime ts_event;
    cmf::OrderId order_id;
    cmf::Side side;
    cmf::Price price;
    cmf::Quantity size;
    OrderAction action;
};

struct ParseError : std::exception {
    const char *what() const noexcept override { return "Parse error"; }
};

std::string_view trim_quotes(std::string_view sv) {
    if (sv.size() >= 2 && sv.front() == '"' && sv.back() == '"') {
        return sv.substr(1, sv.size() - 2);
    }
    return sv;
}

bool isValidLine(const std::string &line) {
    if (line.empty()) return false;
    const size_t f = line.find('{');
    const size_t l = line.find('}');
    return f != std::string::npos && l != std::string::npos && f <= l;
}

bool isValidMarketData(const MarketDataEvent &event) {
    if (event.order_id == 0) return false;
    if (event.price <= 0) return false;
    if (event.size <= 0) return false;
    if (event.action == OrderAction::None) return false;
    return true;
}

template<typename T>
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

    cmf::OrderId order_id = 0;
    int side = 0;
    cmf::Price price = 0;
    cmf::Quantity size = 0;
    int action = 0;

    auto ts_sv = extract_field(line, "\"ts_event\"");
    auto id_sv = trim_quotes(extract_field(line, "\"order_id\""));
    auto side_sv = trim_quotes(extract_field(line, "\"side\""));
    auto price_sv = trim_quotes(extract_field(line, "\"price\""));
    auto size_sv = extract_field(line, "\"size\"");
    auto action_sv = trim_quotes(extract_field(line, "\"action\""));

    if (!parse_ts_event(ts_sv, e.ts_event)) throw ParseError{};
    if (!parse_number(id_sv, order_id)) throw ParseError{};

    if (side_sv == "A") {
        side = static_cast<int>(cmf::Side::Sell);
    } else if (side_sv == "B") {
        side = static_cast<int>(cmf::Side::Buy);
    } else if (side_sv == "N") {
        side = static_cast<int>(cmf::Side::None);
    } else {
        throw ParseError{};
    }

    if (price_sv == "null") throw ParseError{};
    if (!parse_number(price_sv, price)) throw ParseError{};
    if (!parse_number(size_sv, size)) throw ParseError{};

    if (action_sv == "A") action = static_cast<int>(OrderAction::Add);
    else if (action_sv == "R") action = static_cast<int>(OrderAction::Cancel);
    else throw ParseError{};

    e.order_id = order_id;
    e.side = static_cast<cmf::Side>(side);
    e.price = price;
    e.size = size;
    e.action = static_cast<OrderAction>(action);

    return e;
}

void processMarketDataEvent(const MarketDataEvent &order) {
    std::cout << static_cast<long long>(order.ts_event) << ", "
            << order.order_id << ", "
            << static_cast<int>(order.side) << ", "
            << order.price << ", "
            << order.size << ", "
            << static_cast<int>(order.action) << '\n';
}

void print_event_list(const char *title, const std::vector<MarketDataEvent> &events) {
    std::cout << title << '\n';
    for (const auto &e: events) {
        processMarketDataEvent(e);
    }
}

void print_event_list(const char *title, const std::deque<MarketDataEvent> &events) {
    std::cout << title << '\n';
    for (const auto &e: events) {
        processMarketDataEvent(e);
    }
}

int main([[maybe_unused]] int argc, [[maybe_unused]] const char *argv[]) {
    try {
        if (argc < 2) {
            std::cerr << "FILE NOT PROVIDED";
            return 1;
        }

        std::ifstream file(argv[1]);
        if (!file.is_open()) {
            std::cerr << "FAILED TO OPEN FILE";
            return 1;
        }

        std::string line;

        std::vector<MarketDataEvent> first_events;
        std::deque<MarketDataEvent> last_events;

        std::size_t total_messages = 0;
        cmf::NanoTime first_ts = 0;
        cmf::NanoTime last_ts = 0;
        bool have_ts = false;

        while (std::getline(file, line)) {
            if (!isValidLine(line))
                continue;

            try {
                auto event = parse_event(line);
                if (!isValidMarketData(event)) continue;

                processMarketDataEvent(event);

                if (first_events.size() < 10) {
                    first_events.push_back(event);
                }

                if (last_events.size() == 10) {
                    last_events.pop_front();
                }
                last_events.push_back(event);

                if (!have_ts) {
                    first_ts = event.ts_event;
                    have_ts = true;
                }
                last_ts = event.ts_event;

                ++total_messages;
            } catch (const ParseError &) {
                continue;
            }
        }

        std::cout << "summary\n";
        std::cout << "total messages processed: " << total_messages << '\n';

        if (have_ts) {
            std::cout << "first timestamp: " << static_cast<long long>(first_ts) << '\n';
            std::cout << "last timestamp: " << static_cast<long long>(last_ts) << '\n';
        } else {
            std::cout << "first timestamp: n/a\n";
            std::cout << "last timestamp: n/a\n";
        }

        print_event_list("first 10 MarketDataEvent objects:", first_events);
        print_event_list("last 10 MarketDataEvent objects:", last_events);
    } catch (std::exception &ex) {
        std::cerr << "Back-tester threw an exception: " << ex.what() << std::endl;
        return 1;
    }
    return 0;
}
