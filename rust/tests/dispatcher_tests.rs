use back_tester::dispatcher::Dispatcher;
use back_tester::model::{Action, MarketDataEvent, Side};
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
        symbol: format!("INST-{}", instrument_id),
    }
}

#[test]
fn dispatch_creates_book_for_instrument() {
    let mut dispatcher = Dispatcher::new(1_000_000);
    dispatcher.dispatch(&make_event(100, Action::Add, Side::Bid, Some("50.0"), 10, 1));

    assert_eq!(dispatcher.book_count(), 1);
    assert!(dispatcher.get_book(100).is_some());
    assert!(dispatcher.get_book(999).is_none());
}

#[test]
fn dispatch_multiple_instruments_separate_books() {
    let mut dispatcher = Dispatcher::new(1_000_000);
    dispatcher.dispatch(&make_event(100, Action::Add, Side::Bid, Some("50.0"), 10, 1));
    dispatcher.dispatch(&make_event(200, Action::Add, Side::Ask, Some("60.0"), 20, 2));
    dispatcher.dispatch(&make_event(100, Action::Add, Side::Ask, Some("51.0"), 15, 3));

    assert_eq!(dispatcher.book_count(), 2);

    let book100 = dispatcher.get_book(100).unwrap();
    assert_eq!(book100.order_count(), 2);
    assert_eq!(
        book100.best_bid().unwrap().0,
        Decimal::from_str("50.0").unwrap()
    );
    assert_eq!(
        book100.best_ask().unwrap().0,
        Decimal::from_str("51.0").unwrap()
    );

    let book200 = dispatcher.get_book(200).unwrap();
    assert_eq!(book200.order_count(), 1);
    assert_eq!(
        book200.best_ask().unwrap().0,
        Decimal::from_str("60.0").unwrap()
    );
}

#[test]
fn dispatch_tracks_total_events() {
    let mut dispatcher = Dispatcher::new(1_000_000);
    dispatcher.dispatch(&make_event(100, Action::Add, Side::Bid, Some("50.0"), 10, 1));
    dispatcher.dispatch(&make_event(100, Action::Add, Side::Ask, Some("51.0"), 10, 2));
    dispatcher.dispatch(&make_event(100, Action::Cancel, Side::Bid, Some("50.0"), 10, 1));

    assert_eq!(dispatcher.total_events(), 3);
}

#[test]
fn dispatch_snapshot_interval() {
    let mut dispatcher = Dispatcher::new(2);
    dispatcher.dispatch(&make_event(100, Action::Add, Side::Bid, Some("50.0"), 10, 1));
    dispatcher.dispatch(&make_event(100, Action::Add, Side::Ask, Some("51.0"), 10, 2));
    assert_eq!(dispatcher.total_events(), 2);
}

#[test]
fn print_final_summary_no_panic() {
    let mut dispatcher = Dispatcher::new(1_000_000);
    dispatcher.dispatch(&make_event(100, Action::Add, Side::Bid, Some("50.0"), 10, 1));
    dispatcher.dispatch(&make_event(200, Action::Add, Side::Ask, Some("60.0"), 20, 2));
    dispatcher.print_final_summary();
}

#[test]
fn empty_dispatcher() {
    let dispatcher = Dispatcher::new(1_000_000);
    assert_eq!(dispatcher.book_count(), 0);
    assert_eq!(dispatcher.total_events(), 0);
    dispatcher.print_final_summary();
}
