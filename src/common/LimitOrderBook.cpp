#include "common/LimitOrderBook.hpp"

#include <iomanip>
#include <ostream>

namespace cmf {
    namespace {
        constexpr std::int64_t UNDEF = MarketDataEvent::UNDEF_PRICE;
    }

    void LimitOrderBook::addToLevel(SideChar side, std::int64_t price, std::int64_t qty) {
        if (side == SideChar::Buy) {
            bids_[price] += qty;
        } else if (side == SideChar::Ask) {
            asks_[price] += qty;
        }
    }

    void LimitOrderBook::removeFromLevel(SideChar side, std::int64_t price, std::int64_t qty) {
        if (side == SideChar::Buy) {
            auto it = bids_.find(price);
            if (it == bids_.end()) {
                return;
            }
            it->second -= qty;
            if (it->second <= 0) {
                bids_.erase(it);
            }
        } else if (side == SideChar::Ask) {
            auto it = asks_.find(price);
            if (it == asks_.end()) {
                return;
            }
            it->second -= qty;
            if (it->second <= 0) {
                asks_.erase(it);
            }
        }
    }

    bool LimitOrderBook::apply(const MarketDataEvent &ev) {
        switch (ev.action) {
            case Action::Add: {
                if (ev.side == SideChar::None || ev.price == UNDEF) {
                    return false;
                }
                orders_[ev.order_id] = OrderInfo{ev.price, ev.size, ev.side};
                addToLevel(ev.side, ev.price, ev.size);
                return true;
            }

            case Action::Cancel: {
                auto it = orders_.find(ev.order_id);
                if (it == orders_.end()) {
                    return false;
                }
                auto &info = it->second;


                std::int64_t qty = ev.size;
                if (qty <= 0 || qty > info.size) {
                    qty = info.size;
                }
                removeFromLevel(info.side, info.price, qty);
                info.size -= qty;
                if (info.size <= 0) {
                    orders_.erase(it);
                }
                return true;
            }

            case Action::Modify: {
                auto it = orders_.find(ev.order_id);
                if (it != orders_.end()) {
                    removeFromLevel(it->second.side, it->second.price, it->second.size);
                }
                if (ev.side == SideChar::None || ev.price == UNDEF) {
                    if (it != orders_.end()) {
                        orders_.erase(it);
                    }
                    return true;
                }

                orders_[ev.order_id] = OrderInfo{ev.price, ev.size, ev.side};
                addToLevel(ev.side, ev.price, ev.size);
                return true;
            }

            case Action::Trade:
            case Action::Fill:


                return false;

            case Action::Clear:
                clear();
                return true;

            case Action::None:
            default:
                return false;
        }
    }

    std::int64_t LimitOrderBook::bestBid() const {
        if (bids_.empty()) return UNDEF;
        return bids_.begin()->first;
    }

    std::int64_t LimitOrderBook::bestAsk() const {
        if (asks_.empty()) return UNDEF;
        return asks_.begin()->first;
    }

    std::int64_t LimitOrderBook::volumeAtPrice(SideChar side, std::int64_t price) const {
        if (side == SideChar::Buy) {
            auto it = bids_.find(price);
            return it == bids_.end() ? 0 : it->second;
        } else if (side == SideChar::Ask) {
            auto it = asks_.find(price);
            return it == asks_.end() ? 0 : it->second;
        }
        return 0;
    }

    void LimitOrderBook::clear() {
        orders_.clear();
        bids_.clear();
        asks_.clear();
    }

    void LimitOrderBook::printSnapshot(std::ostream &os, std::size_t depth) const {
        auto fmtPx = [](std::int64_t p) -> double {
            return static_cast<double>(p) / 1e9;
        };

        os << "    BID                  |  ASK\n";
        os << "    qty       price      |  price          qty\n";
        os << "    --------------------- + -----------------------\n";

        auto bit = bids_.begin();
        auto ait = asks_.begin();
        for (std::size_t i = 0; i < depth; ++i) {
            bool has_b = bit != bids_.end();
            bool has_a = ait != asks_.end();
            if (!has_b && !has_a) break;

            os << "    ";
            if (has_b) {
                os << std::setw(8) << bit->second << "  "
                        << std::fixed << std::setprecision(9) << std::setw(11) << fmtPx(bit->first);
                ++bit;
            } else {
                os << std::string(21, ' ');
            }
            os << "  |  ";
            if (has_a) {
                os << std::fixed << std::setprecision(9) << std::setw(11) << fmtPx(ait->first)
                        << "  " << std::setw(8) << ait->second;
                ++ait;
            }
            os << "\n";
        }
    }
} // namespace cmf
