#include "common/EventConsumer.hpp"

#include <iomanip>
#include <iostream>

namespace cmf {
    namespace {
        constexpr std::size_t kPrintFirstN = 0;

        void printNanoTime(std::ostream &os, NanoTime ns) {
            os << ns;
        }

        const char *actionToStr(Action a) {
            switch (a) {
                case Action::Add: return "Add";
                case Action::Modify: return "Mod";
                case Action::Cancel: return "Cnc";
                case Action::Trade: return "Trd";
                case Action::Fill: return "Fil";
                case Action::Clear: return "Clr";
                case Action::None: return "Non";
            }
            return "?";
        }

        const char *sideToStr(SideChar s) {
            switch (s) {
                case SideChar::Buy: return "Buy";
                case SideChar::Ask: return "Ask";
                case SideChar::None: return "Non";
            }
            return "?";
        }
    } // namespace

    void printEvent(std::ostream &os, const MarketDataEvent &ev) {
        os << "ts_event=";
        printNanoTime(os, ev.ts_event);
        os << " instr=" << ev.instrument_id
                << " act=" << actionToStr(ev.action)
                << " side=" << sideToStr(ev.side)
                << " ord=" << ev.order_id;

        if (ev.price != MarketDataEvent::UNDEF_PRICE) {
            os << std::fixed << std::setprecision(9)
                    << " px=" << (static_cast<double>(ev.price) / 1e9);
        } else {
            os << " px=---";
        }
        os << " sz=" << ev.size;
        if (!ev.symbol.empty()) {
            os << " sym=\"" << ev.symbol << "\"";
        }
    }

    void processMarketDataEvent(const MarketDataEvent &ev) {
        static std::size_t printed = 0;
        if (printed < kPrintFirstN) {
            printEvent(std::cout, ev);
            std::cout << "\n";
            ++printed;
        }
    }
} // namespace cmf
