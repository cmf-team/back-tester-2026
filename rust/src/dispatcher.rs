use std::collections::HashMap;

use crate::model::MarketDataEvent;
use crate::orderbook::LimitOrderBook;

/// Routes MarketDataEvents to per-instrument LimitOrderBooks
pub struct Dispatcher {
    books: HashMap<u32, LimitOrderBook>,
    event_count: u64,
    snapshot_interval: u64,
}

impl Dispatcher {
    pub fn new(snapshot_interval: u64) -> Self {
        Self {
            books: HashMap::new(),
            event_count: 0,
            snapshot_interval,
        }
    }

    /// Route an event to the appropriate instrument's LOB
    pub fn dispatch(&mut self, event: &MarketDataEvent) {
        let book = self
            .books
            .entry(event.instrument_id)
            .or_insert_with(|| LimitOrderBook::new(event.instrument_id));

        book.apply(event);
        self.event_count += 1;

        if self.snapshot_interval > 0 && self.event_count.is_multiple_of(self.snapshot_interval) {
            println!("\n--- Snapshot at event #{} ---", self.event_count);
            book.print_snapshot(5);
        }
    }

    pub fn get_book(&self, instrument_id: u32) -> Option<&LimitOrderBook> {
        self.books.get(&instrument_id)
    }

    pub fn book_count(&self) -> usize {
        self.books.len()
    }

    pub fn total_events(&self) -> u64 {
        self.event_count
    }

    /// Print final best bid/ask for every instrument
    pub fn print_final_summary(&self) {
        println!("\n{}", "=".repeat(70));
        println!("FINAL LOB SUMMARY — {} instruments", self.books.len());
        println!("{}", "=".repeat(70));

        let mut ids: Vec<_> = self.books.keys().collect();
        ids.sort();

        for id in ids {
            let book = &self.books[id];
            let symbol = book.symbol().unwrap_or("unknown");
            let bid_str = match book.best_bid() {
                Some((p, s)) => format!("{} x {}", p.normalize(), s),
                None => "empty".to_string(),
            };
            let ask_str = match book.best_ask() {
                Some((p, s)) => format!("{} x {}", p.normalize(), s),
                None => "empty".to_string(),
            };
            println!(
                "  [{:>6}] {:<40} Bid: {:>20}  |  Ask: {:>20}",
                id, symbol, bid_str, ask_str
            );
        }
    }
}
