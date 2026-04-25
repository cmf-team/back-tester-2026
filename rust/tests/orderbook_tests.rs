use back_tester::model::{Action, MarketDataEvent, Side};
use back_tester::orderbook::LimitOrderBook;
use rust_decimal::Decimal;
use std::str::FromStr;

fn make_event(
    instrument_id: u32,
    action: Action,
    side: Side,
    price: Option<&str>,
    size: u32,
    order_id: u64,
) -> MarketDataEvent {
    MarketDataEvent {
        ts_recv: "2026-03-10T08:00:00.000000000Z".parse().unwrap(),
        ts_event: "2026-03-10T08:00:00.000000000Z".parse().unwrap(),
        rtype: 160,
        publisher_id: 101,
        instrument_id,
        action,
        side,
        price: price.map(|p| Decimal::from_str(p).unwrap()),
        size,
        channel_id: 1,
        order_id,
        flags: 0,
        ts_in_delta: 0,
        sequence: 0,
        symbol: "TEST".to_string(),
    }
}

#[test]
fn add_single_bid() {
    let mut book = LimitOrderBook::new(1);
    let event = make_event(1, Action::Add, Side::Bid, Some("100.5"), 10, 1001);
    book.apply(&event);

    let best = book.best_bid().unwrap();
    assert_eq!(best.0, Decimal::from_str("100.5").unwrap());
    assert_eq!(best.1, 10);
    assert_eq!(book.order_count(), 1);
    assert_eq!(book.bid_levels(), 1);
}

#[test]
fn add_single_ask() {
    let mut book = LimitOrderBook::new(1);
    let event = make_event(1, Action::Add, Side::Ask, Some("101.0"), 5, 1002);
    book.apply(&event);

    let best = book.best_ask().unwrap();
    assert_eq!(best.0, Decimal::from_str("101.0").unwrap());
    assert_eq!(best.1, 5);
    assert_eq!(book.order_count(), 1);
    assert_eq!(book.ask_levels(), 1);
}

#[test]
fn add_multiple_bids_best_is_highest() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("99.0"), 10, 1));
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("101.0"), 20, 2));
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 15, 3));

    let best = book.best_bid().unwrap();
    assert_eq!(best.0, Decimal::from_str("101.0").unwrap());
    assert_eq!(best.1, 20);
    assert_eq!(book.bid_levels(), 3);
}

#[test]
fn add_multiple_asks_best_is_lowest() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Ask, Some("103.0"), 10, 1));
    book.apply(&make_event(1, Action::Add, Side::Ask, Some("101.0"), 20, 2));
    book.apply(&make_event(1, Action::Add, Side::Ask, Some("102.0"), 15, 3));

    let best = book.best_ask().unwrap();
    assert_eq!(best.0, Decimal::from_str("101.0").unwrap());
    assert_eq!(best.1, 20);
    assert_eq!(book.ask_levels(), 3);
}

#[test]
fn add_multiple_orders_same_price_aggregates() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 10, 1));
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 25, 2));

    let best = book.best_bid().unwrap();
    assert_eq!(best.0, Decimal::from_str("100.0").unwrap());
    assert_eq!(best.1, 35);
    assert_eq!(book.bid_levels(), 1);
    assert_eq!(book.order_count(), 2);
}

#[test]
fn add_with_none_price_skipped() {
    let mut book = LimitOrderBook::new(1);
    let event = make_event(1, Action::Add, Side::Bid, None, 10, 1);
    book.apply(&event);

    assert!(book.best_bid().is_none());
    assert_eq!(book.order_count(), 0);
}

#[test]
fn empty_book_queries() {
    let book = LimitOrderBook::new(1);
    assert!(book.best_bid().is_none());
    assert!(book.best_ask().is_none());
    assert_eq!(book.bid_levels(), 0);
    assert_eq!(book.ask_levels(), 0);
    assert_eq!(book.order_count(), 0);
    assert_eq!(
        book.volume_at_price(Side::Bid, &Decimal::from_str("100.0").unwrap()),
        0
    );
}

#[test]
fn volume_at_price_existing_level() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Ask, Some("50.0"), 100, 1));

    assert_eq!(
        book.volume_at_price(Side::Ask, &Decimal::from_str("50.0").unwrap()),
        100
    );
    assert_eq!(
        book.volume_at_price(Side::Ask, &Decimal::from_str("51.0").unwrap()),
        0
    );
}

#[test]
fn cancel_existing_order() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 10, 1));
    book.apply(&make_event(1, Action::Cancel, Side::Bid, Some("100.0"), 10, 1));

    assert!(book.best_bid().is_none());
    assert_eq!(book.order_count(), 0);
    assert_eq!(book.bid_levels(), 0);
}

#[test]
fn cancel_last_order_at_level_removes_level() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Ask, Some("50.0"), 10, 1));
    book.apply(&make_event(1, Action::Cancel, Side::Ask, Some("50.0"), 10, 1));

    assert_eq!(book.ask_levels(), 0);
    assert_eq!(
        book.volume_at_price(Side::Ask, &Decimal::from_str("50.0").unwrap()),
        0
    );
}

#[test]
fn cancel_unknown_order_no_panic() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 10, 1));
    book.apply(&make_event(1, Action::Cancel, Side::Bid, Some("100.0"), 10, 9999));

    assert_eq!(book.order_count(), 1);
    assert_eq!(book.best_bid().unwrap().1, 10);
}

#[test]
fn cancel_one_of_multiple_at_same_level() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 10, 1));
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 25, 2));
    book.apply(&make_event(1, Action::Cancel, Side::Bid, Some("100.0"), 10, 1));

    assert_eq!(book.order_count(), 1);
    assert_eq!(book.best_bid().unwrap().1, 25);
    assert_eq!(book.bid_levels(), 1);
}

#[test]
fn modify_price_moves_order() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 10, 1));
    book.apply(&make_event(1, Action::Modify, Side::Bid, Some("101.0"), 10, 1));

    assert_eq!(
        book.volume_at_price(Side::Bid, &Decimal::from_str("100.0").unwrap()),
        0
    );
    assert_eq!(
        book.volume_at_price(Side::Bid, &Decimal::from_str("101.0").unwrap()),
        10
    );
    assert_eq!(
        book.best_bid().unwrap().0,
        Decimal::from_str("101.0").unwrap()
    );
    assert_eq!(book.order_count(), 1);
}

#[test]
fn modify_size_at_same_price() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Ask, Some("50.0"), 10, 1));
    book.apply(&make_event(1, Action::Modify, Side::Ask, Some("50.0"), 30, 1));

    assert_eq!(book.best_ask().unwrap().1, 30);
    assert_eq!(book.order_count(), 1);
}

#[test]
fn modify_unknown_order_treated_as_add() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Modify, Side::Bid, Some("99.0"), 15, 9999));

    assert_eq!(
        book.best_bid().unwrap().0,
        Decimal::from_str("99.0").unwrap()
    );
    assert_eq!(book.best_bid().unwrap().1, 15);
    assert_eq!(book.order_count(), 1);
}

#[test]
fn clear_removes_everything() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 10, 1));
    book.apply(&make_event(1, Action::Add, Side::Ask, Some("101.0"), 20, 2));
    book.apply(&make_event(1, Action::Clear, Side::None, None, 0, 0));

    assert!(book.best_bid().is_none());
    assert!(book.best_ask().is_none());
    assert_eq!(book.order_count(), 0);
    assert_eq!(book.bid_levels(), 0);
    assert_eq!(book.ask_levels(), 0);
}

#[test]
fn clear_empty_book_no_panic() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Clear, Side::None, None, 0, 0));
    assert_eq!(book.order_count(), 0);
}

#[test]
fn add_after_clear_works() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 10, 1));
    book.apply(&make_event(1, Action::Clear, Side::None, None, 0, 0));
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("99.0"), 5, 2));

    assert_eq!(
        book.best_bid().unwrap().0,
        Decimal::from_str("99.0").unwrap()
    );
    assert_eq!(book.order_count(), 1);
}

#[test]
fn trade_does_not_affect_book() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 10, 1));
    book.apply(&make_event(1, Action::Trade, Side::None, Some("100.0"), 5, 0));

    assert_eq!(book.order_count(), 1);
    assert_eq!(book.best_bid().unwrap().1, 10);
}

#[test]
fn fill_does_not_affect_book() {
    let mut book = LimitOrderBook::new(1);
    book.apply(&make_event(1, Action::Add, Side::Ask, Some("101.0"), 20, 1));
    book.apply(&make_event(1, Action::Fill, Side::Ask, Some("101.0"), 5, 1));

    assert_eq!(book.order_count(), 1);
    assert_eq!(book.best_ask().unwrap().1, 20);
}

#[test]
fn print_snapshot_no_panic() {
    let mut book = LimitOrderBook::new(1);
    book.print_snapshot(5);
    book.apply(&make_event(1, Action::Add, Side::Bid, Some("100.0"), 10, 1));
    book.apply(&make_event(1, Action::Add, Side::Ask, Some("101.0"), 20, 2));
    book.print_snapshot(5);
}
