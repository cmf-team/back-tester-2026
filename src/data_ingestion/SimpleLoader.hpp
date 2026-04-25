#pragma once

#include "common/MarketDataEvent.hpp"
#include "parsers/json/JSONParser.hpp"
#include <fstream>
#include <stdexcept>
#include <string>

namespace cmf {

class SimpleLoader {
public:
    explicit SimpleLoader(const std::string& path) : path_(path) {}

    // Reads the file line-by-line (NDJSON).
    // For each parseable line, creates a MarketDataEvent and calls process().
    // Malformed lines are silently skipped.
    template<typename Consumer>
    void load(Consumer&& process) {
        std::ifstream file(path_);
        if (!file.is_open())
            throw std::runtime_error("Cannot open file: " + path_);

        parser::MDParser& base = parser_;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            try {
                process(base.parse_line(line));
            } catch (...) {}
        }
    }

private:
    std::string path_;
    parser::JSONParser parser_;
};

} // namespace cmf
