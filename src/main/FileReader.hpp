#pragma once

#include <cstddef>
#include <fstream>
#include <string>

namespace cmf
{

class FileReader
{
  public:
    explicit FileReader(const std::string &inputFilePath);
    bool readNextRawLine(std::string &rawLine);

  private:
    std::ifstream inputFile_;
    std::string inputFilePath_;
    std::size_t lineNumber_ = 0;
};

} // namespace cmf
