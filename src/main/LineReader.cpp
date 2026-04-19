#include "LineReader.hpp"
#include <stdexcept>

using namespace cmf;

LineReader::LineReader(const std::string& path) : stream_(path) {
    if (!stream_.is_open()) {
        throw std::runtime_error("LineReader: failed to open " + path);
    }
}

bool LineReader::nextLine(std::string& out) {
    return static_cast<bool>(std::getline(stream_, out));
}


