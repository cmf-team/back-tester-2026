#pragma once
#include <string>

struct MarketDataEvent;

class Summary {
private:
    int total = 0;
    std::string first_ts;
    std::string last_ts;

public:
    void update(const MarketDataEvent& e);
    void print() const;

    int getTotal() const;
    std::string getFirstTs() const;
    std::string getLastTs() const;
};