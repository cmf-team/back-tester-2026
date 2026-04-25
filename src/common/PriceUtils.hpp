#pragma once

#include <cctype>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>

#include "BasicTypes.hpp"

namespace cmf {

inline Price parseScaledPrice(const std::string& s) {
    if (s.empty()) {
        throw std::runtime_error("Empty price string");
    }

    bool negative = false;
    std::size_t i = 0;

    if (s[i] == '-') {
        negative = true;
        ++i;
    }

    std::int64_t integerPart = 0;
    while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
        integerPart = integerPart * 10 + (s[i] - '0');
        ++i;
    }

    std::int64_t fractionalPart = 0;
    std::int64_t fractionalScale = 1;

    if (i < s.size() && s[i] == '.') {
        ++i;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            if (fractionalScale < PriceScale) {
                fractionalPart = fractionalPart * 10 + (s[i] - '0');
                fractionalScale *= 10;
            }
            ++i;
        }
    }

    while (fractionalScale < PriceScale) {
        fractionalPart *= 10;
        fractionalScale *= 10;
    }

    std::int64_t result = integerPart * PriceScale + fractionalPart;
    return negative ? -result : result;
}

inline std::string formatScaledPrice(Price price) {
    bool negative = price < 0;
    std::int64_t absPrice = negative ? -price : price;

    std::int64_t integerPart = absPrice / PriceScale;
    std::int64_t fractionalPart = absPrice % PriceScale;

    std::ostringstream oss;
    if (negative) {
        oss << '-';
    }

    oss << integerPart << '.'
        << std::setw(9) << std::setfill('0') << fractionalPart;

    return oss.str();
}

} // namespace cmf