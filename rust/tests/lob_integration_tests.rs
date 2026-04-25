use std::io::{BufRead, BufReader};
use std::path::PathBuf;

use back_tester::dispatcher::Dispatcher;
use back_tester::parser::parse_line;
use rust_decimal::Decimal;
use std::str::FromStr;

fn fixture_path(name: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(name)
}

fn run_file_through_dispatcher(path: &std::path::Path, snapshot_interval: u64) -> Dispatcher {
    let file = std::fs::File::open(path).unwrap();
    let reader = BufReader::new(file);
    let mut dispatcher = Dispatcher::new(snapshot_interval);

    for line_result in reader.lines() {
        let line = line_result.unwrap();
        if line.trim().is_empty() {
            continue;
        }
        if let Ok(event) = parse_line(&line) {
            dispatcher.dispatch(&event);
        }
    }

    dispatcher
}

#[test]
fn orderbook_scenario_final_state() {
    let dispatcher = run_file_through_dispatcher(
        &fixture_path("orderbook_scenario.json"),
        1_000_000,
    );

    assert_eq!(dispatcher.total_events(), 9);
    assert_eq!(dispatcher.book_count(), 2);

    // Instrument 100: after Clear, 2 bids added, 2 asks added,
    // bid 1001 modified from 99.5->99.75, bid 1002 cancelled, trade (no effect)
    let book100 = dispatcher.get_book(100).unwrap();
    assert_eq!(book100.order_count(), 3); // 1001(modified), 2001, 2002
    assert_eq!(
        book100.best_bid().unwrap().0,
        Decimal::from_str("99.75").unwrap()
    );
    assert_eq!(book100.best_bid().unwrap().1, 10);
    assert_eq!(
        book100.best_ask().unwrap().0,
        Decimal::from_str("100.5").unwrap()
    );
    assert_eq!(book100.best_ask().unwrap().1, 15);
    assert_eq!(book100.bid_levels(), 1); // only 99.75 remains
    assert_eq!(book100.ask_levels(), 2); // 100.5, 101.0

    // Instrument 200: one bid added
    let book200 = dispatcher.get_book(200).unwrap();
    assert_eq!(book200.order_count(), 1);
    assert_eq!(
        book200.best_bid().unwrap().0,
        Decimal::from_str("1.15").unwrap()
    );
    assert_eq!(book200.best_bid().unwrap().1, 50);
}

#[test]
fn empty_file_no_panic() {
    let dispatcher = run_file_through_dispatcher(&fixture_path("empty.json"), 1_000_000);
    assert_eq!(dispatcher.total_events(), 0);
    assert_eq!(dispatcher.book_count(), 0);
}

#[test]
fn print_final_summary_no_panic() {
    let dispatcher = run_file_through_dispatcher(
        &fixture_path("orderbook_scenario.json"),
        1_000_000,
    );
    dispatcher.print_final_summary();
}

#[test]
fn mixed_fixture_creates_multiple_books() {
    let dispatcher = run_file_through_dispatcher(
        &fixture_path("sample_mixed.json"),
        1_000_000,
    );
    // sample_mixed.json has instrument_ids: 34513, 34357, 442, 34257
    assert!(dispatcher.book_count() >= 3);
    assert!(dispatcher.total_events() >= 7);
}
