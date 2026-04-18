#pragma once
#include "models/MarketDataEvent.h"

void processMarketDataEvent(const MarketDataEvent& event, MarketDataEventSumary& summary);