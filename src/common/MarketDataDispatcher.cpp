#include "common/MarketDataDispatcher.hpp"

#include <iomanip>
#include <ostream>

namespace cmf {
    namespace {
        const std::string kEmpty;
    }

    LimitOrderBook &MarketDataDispatcher::book(std::uint32_t instrument_id) {
        return books_[instrument_id];
    }

    const LimitOrderBook *MarketDataDispatcher::find(std::uint32_t instrument_id) const {
        auto it = books_.find(instrument_id);
        return it == books_.end() ? nullptr : &it->second;
    }

    void MarketDataDispatcher::rememberSymbol(std::uint32_t instrument_id,
                                              const std::string &symbol) {
        if (!symbol.empty()) {
            symbols_.try_emplace(instrument_id, symbol);
        }
    }

    const std::string &MarketDataDispatcher::symbolOf(std::uint32_t instrument_id) const {
        auto it = symbols_.find(instrument_id);
        return it == symbols_.end() ? kEmpty : it->second;
    }

    bool MarketDataDispatcher::dispatch(const MarketDataEvent &ev) {
        if (!ev.symbol.empty()) {
            rememberSymbol(ev.instrument_id, ev.symbol);
        }


        std::uint32_t target = ev.instrument_id;

        if (ev.action == Action::Cancel || ev.action == Action::Modify) {
            auto it = order_to_instr_.find(ev.order_id);
            if (it != order_to_instr_.end()) {
                target = it->second;
            }
        }

        if (target == 0) {
            return false;
        }

        auto &lob = book(target);
        bool changed = lob.apply(ev);


        if (ev.action == Action::Add) {
            order_to_instr_[ev.order_id] = target;
        } else if (ev.action == Action::Cancel) {
            order_to_instr_.erase(ev.order_id);
        } else if (ev.action == Action::Clear) {
        }

        return changed;
    }

    void MarketDataDispatcher::printSnapshot(std::ostream &os,
                                             std::uint32_t instrument_id,
                                             std::size_t depth) const {
        const auto *lob = find(instrument_id);
        if (lob == nullptr) {
            os << "[no book for instrument " << instrument_id << "]\n";
            return;
        }
        os << "Snapshot for instrument " << instrument_id;
        const auto &sym = symbolOf(instrument_id);
        if (!sym.empty()) {
            os << " (" << sym << ")";
        }
        os << "\n  bid levels=" << lob->bidLevels()
                << " ask levels=" << lob->askLevels()
                << " orders=" << lob->orderCount() << "\n";
        lob->printSnapshot(os, depth);
    }

    void MarketDataDispatcher::printAllBestQuotes(std::ostream &os) const {
        os << "Final best bid/ask per instrument:\n";
        os << std::left << std::setw(10) << "instr"
                << std::setw(40) << "symbol"
                << std::right << std::setw(15) << "best_bid"
                << std::setw(15) << "best_ask" << "\n";
        for (const auto &[id, lob]: books_) {
            auto bb = lob.bestBid();
            auto ba = lob.bestAsk();
            os << std::left << std::setw(10) << id
                    << std::setw(40) << symbolOf(id);
            os << std::right;
            if (bb != MarketDataEvent::UNDEF_PRICE) {
                os << std::fixed << std::setprecision(9) << std::setw(15)
                        << static_cast<double>(bb) / 1e9;
            } else {
                os << std::setw(15) << "-";
            }
            if (ba != MarketDataEvent::UNDEF_PRICE) {
                os << std::fixed << std::setprecision(9) << std::setw(15)
                        << static_cast<double>(ba) / 1e9;
            } else {
                os << std::setw(15) << "-";
            }
            os << "\n";
        }
    }
} // namespace cmf
