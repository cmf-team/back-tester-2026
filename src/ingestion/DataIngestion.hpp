#pragma once

#include "common/MarketDataEvent.hpp"
#include <functional>
#include <string>
#include <vector>

using EventCallback = std::function<void(const MarketDataEvent&)>;

void ProcessMarketDataEvent(const MarketDataEvent& event);

auto ListNDJSONFiles(const std::string& folder_path) -> std::vector<std::string>;

class FlatMergerEngine
{
  public:
    void Ingest(const std::vector<std::string>& file_paths, EventCallback on_event);
};

class HierarchyMergerEngine
{
  public:
    void Ingest(const std::vector<std::string>& file_paths, EventCallback on_event);
};
