#pragma once

#include "common/MarketDataEvent.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace cmf
{

class MdEventConverter
{
  public:
    bool parseRaw(const std::string &rawLine, MarketDataEvent &event) const;

    static std::optional<MdAction> actionFromChar(char value);
    static std::optional<Side> sideFromChar(char value);
    static bool isValidActionSide(MdAction action, Side side);
    static char toChar(MdAction value);
    static char toChar(Side value);

    static bool isoTimestampToNanos(std::string_view value, std::int64_t &outUnixNanos);
    static void nanosToIsoTimestamp(std::int64_t unixNanos, char (&buf)[31]);

  private:
    static std::optional<Price> parsePriceDecimalString(const std::string &raw);
};

} // namespace cmf
