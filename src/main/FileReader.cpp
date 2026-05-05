#include "main/FileReader.hpp"

#include <stdexcept>
#include <string>

namespace cmf
{

FileReader::FileReader(const std::string &inputFilePath)
    : inputFile_(inputFilePath), inputFilePath_(inputFilePath)
{
    if (!inputFile_.is_open())
    {
        throw std::runtime_error("Failed to open file: " + inputFilePath);
    }
}

bool FileReader::readNextRawLine(std::string &rawLine)
{
    if (std::getline(inputFile_, rawLine))
    {
        ++lineNumber_;
        return true;
    }

    if (!inputFile_.eof())
    {
        throw std::runtime_error("Error while reading file: " + inputFilePath_);
    }

    return false;
}

} // namespace cmf
