#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>

namespace md {

enum class Side : char {
    Ask = 'A',
    Bid = 'B',
    None = 'N'
};

enum class Action : char {
    Add = 'A',
    Modify = 'M',
    Cancel = 'C',
    Clear = 'R',
    Trade = 'T',
    Fill = 'F',
    None = 'N'
};

struct MarketDataEvent {
    std::uint64_t timestamp{};   // Databento index timestamp from ts_recv.
    std::uint64_t ts_recv{};
    std::uint64_t ts_event{};

    std::uint64_t order_id{};
    Side side{Side::None};
    std::int64_t price{};
    std::uint64_t size{};
    Action action{Action::None};

    std::uint64_t instrument_id{};

    // Stable source metadata used as a deterministic tie-breaker during multi-file merges.
    std::uint32_t source_file_id{};
    std::uint64_t source_sequence{};

    // Kept for diagnostics and for the Standard task's single-file path.
    std::size_t line_number{};
};

char toChar(Side side);
char toChar(Action action);

std::string sideName(Side side);
std::string actionName(Action action);
std::string formatPrice(std::int64_t price);
std::string formatEventFields(const MarketDataEvent& event);

bool eventComesBefore(const MarketDataEvent& lhs, const MarketDataEvent& rhs);

std::ostream& operator<<(std::ostream& os, const MarketDataEvent& event);

} // namespace md
