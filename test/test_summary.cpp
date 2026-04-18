#include <iostream>
#include "../src/common/Summary.h"
#include "../src/common/MarketDataEvent.h"

#define CHECK(cond, msg) \
    if (!(cond)) { std::cout << "FAIL: " << msg << "\n"; return 1; }

int main() {
    Summary summary;

    MarketDataEvent e1;
    e1.ts_recv = "2026-03-09T10:00:00Z";

    MarketDataEvent e2;
    e2.ts_recv = "2026-03-09T11:00:00Z";

    summary.update(e1);
    summary.update(e2);

    CHECK(summary.getTotal() == 2, "total");
    CHECK(summary.getFirstTs() == "2026-03-09T10:00:00Z", "first ts");
    CHECK(summary.getLastTs() == "2026-03-09T11:00:00Z", "last ts");

    std::cout << "OK\n";
    return 0;
}