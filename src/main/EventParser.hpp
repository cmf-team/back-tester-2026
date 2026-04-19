#pragma once
#include "MarketDataEvent.hpp"
#include <string>
 namespace cmf{

MarketDataEvent parseEvent(const std::string& jsonLine);
 
} // namespace cmf