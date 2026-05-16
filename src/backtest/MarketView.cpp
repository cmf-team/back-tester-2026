#include "backtest/MarketView.hpp"

#include "domain/MarketDataEvent.hpp"

#include <cstdint>

namespace md {
namespace {

std::int64_t midpoint(std::int64_t bid, std::int64_t ask) {
    return bid + (ask - bid) / 2;
}

std::int64_t weightedMicroprice(
    std::int64_t best_bid,
    std::int64_t best_ask,
    std::uint64_t best_bid_size,
    std::uint64_t best_ask_size
) {
    const auto denominator = static_cast<long double>(best_bid_size) + static_cast<long double>(best_ask_size);
    const auto numerator = static_cast<long double>(best_ask) * static_cast<long double>(best_bid_size)
        + static_cast<long double>(best_bid) * static_cast<long double>(best_ask_size);

    return static_cast<std::int64_t>(numerator / denominator);
}

} // namespace

MarketView makeMarketView(const LimitOrderBook& book, std::uint64_t timestamp) {
    MarketView view;
    view.instrument_id = book.instrumentId();
    view.timestamp = timestamp;
    view.best_bid = book.bestBid();
    view.best_ask = book.bestAsk();

    if (view.best_bid.has_value()) {
        view.best_bid_size = book.volumeAt(Side::Bid, *view.best_bid);
    }
    if (view.best_ask.has_value()) {
        view.best_ask_size = book.volumeAt(Side::Ask, *view.best_ask);
    }

    if (view.best_bid.has_value() && view.best_ask.has_value()) {
        view.spread = *view.best_ask - *view.best_bid;
        view.mid_price = midpoint(*view.best_bid, *view.best_ask);

        if (view.best_bid_size > 0 || view.best_ask_size > 0) {
            view.microprice = weightedMicroprice(
                *view.best_bid,
                *view.best_ask,
                view.best_bid_size,
                view.best_ask_size
            );
        }
    }

    return view;
}

MarketView makeMarketView(
    const BookManager& book_manager,
    std::uint64_t instrument_id,
    std::uint64_t timestamp
) {
    const auto* book = book_manager.findBook(instrument_id);
    if (book == nullptr) {
        MarketView view;
        view.instrument_id = instrument_id;
        view.timestamp = timestamp;
        return view;
    }

    return makeMarketView(*book, timestamp);
}

} // namespace md
