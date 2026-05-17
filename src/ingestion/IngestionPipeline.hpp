#pragma once

#include <filesystem>
#include <functional>
#include <type_traits>

#include "common/MarketDataEvent.hpp"

namespace cmf
{
template <typename T>
concept ParserType = std::is_class_v<T> && requires {
    std::declval<const T&>().parse(
        std::declval<const std::function<void(const MarketDataEvent&)>&>());
};

template <ParserType ParserImpl, typename MergerImpl>
class IngestionPipeline
{
    ParserImpl parser_;
    MergerImpl& merger_;

  public:
    explicit IngestionPipeline(const std::filesystem::path& data_file,
                               MergerImpl& merger)
        : parser_(data_file), merger_(merger) {}

    void ingest()
    {
        parser_.parse([&](const MarketDataEvent& event)
                      { merger_(event); });
    }
};
} // namespace cmf