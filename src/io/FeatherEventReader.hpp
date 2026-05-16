#pragma once

#include "domain/MarketDataEvent.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>

namespace md {

class FeatherEventReader {
public:
    explicit FeatherEventReader(std::filesystem::path file_path);

    void readAll(
        std::uint32_t source_file_id,
        const std::function<void(const MarketDataEvent&)>& on_event
    ) const;

private:
    std::filesystem::path file_path_;
};

} // namespace md
