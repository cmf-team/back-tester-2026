#pragma once

#include "common/MarketDataEvent.hpp"

#include <cstddef>
#include <fstream>
#include <string>

namespace cmf
{

class FileReader
{
  public:
    explicit FileReader(const std::string &inputFilePath);
    bool readNextEvent(MarketDataEvent &event);

  private:
    bool parseEventFromLine(const std::string &line, MarketDataEvent &event) const;

    std::ifstream inputFile_;
    std::string inputFilePath_;
    std::size_t lineNumber_{0};
};

} // namespace cmf
