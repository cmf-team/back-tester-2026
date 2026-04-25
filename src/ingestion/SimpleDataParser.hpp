#pragma once

#include <filesystem>
#include <functional>

#include "common/MarketDataEvent.hpp"

namespace cmf {

class SimpleDataParser {
public:
    explicit SimpleDataParser(std::filesystem::path path)
        : path_(std::move(path)) {}

    void parse_inner(const std::function<void(const MarketDataEvent&)>& f) const;

private:
    std::filesystem::path path_;
};

} // namespace cmf