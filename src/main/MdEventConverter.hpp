#pragma once

#include "common/MarketDataEvent.hpp"

#include <optional>
#include <string>

namespace cmf
{

class MdEventConverter
{
  public:
    bool parseRaw(const std::string &rawLine, MarketDataEvent &event) const;

    static std::optional<MdAction> actionFromChar(char value);
    static std::optional<MdSide> sideFromChar(char value);
    static bool isValidActionSide(MdAction action, MdSide side);
    static char toChar(MdAction value);
    static char toChar(MdSide value);

    static std::string formatUnixNanosToIso8601(std::int64_t unixNanos);

  private:
    static std::optional<std::int64_t> parseIso8601ToUnixNanos(const std::string &value);
    static std::optional<std::int64_t> parsePriceDecimalString(const std::string &raw);
};

} // namespace cmf
