use std::collections::VecDeque;

use chrono::{DateTime, Utc};

use crate::model::{Action, MarketDataEvent};

/// Consumer function that receives every MarketDataEvent for verification
///
/// Prints the event details (timestamp, order_id, side, price, size, action)
/// for the first 10 non-Clear events. The last 10 are printed in the summary
/// after the entire file is processed.
pub fn process_market_data_event(event: &MarketDataEvent, summary: &Summary) {
    if event.action != Action::Clear && summary.displayed_count < BOUNDARY_COUNT {
        println!("{event}");
    }
}

/// Collects summary statistics while processing events
pub struct Summary {
    pub total: usize,
    pub errors: usize,
    pub displayed_count: usize,
    pub first_timestamp: Option<DateTime<Utc>>,
    pub last_timestamp: Option<DateTime<Utc>>,
    pub first_events: Vec<MarketDataEvent>,
    pub last_events: VecDeque<MarketDataEvent>,
}

const BOUNDARY_COUNT: usize = 10;

impl Summary {
    pub fn new() -> Self {
        Self {
            total: 0,
            errors: 0,
            displayed_count: 0,
            first_timestamp: None,
            last_timestamp: None,
            first_events: Vec::with_capacity(BOUNDARY_COUNT),
            last_events: VecDeque::with_capacity(BOUNDARY_COUNT),
        }
    }

    /// Record a successfully parsed event
    pub fn record(&mut self, event: &MarketDataEvent) {
        self.total += 1;

        let ts = event.ts_recv;
        if self.first_timestamp.is_none() {
            self.first_timestamp = Some(ts);
        }
        self.last_timestamp = Some(ts);

        // Skip Clear events for the first/last 10 boundary display
        if event.action == Action::Clear {
            return;
        }

        self.displayed_count += 1;

        if self.first_events.len() < BOUNDARY_COUNT {
            self.first_events.push(event.clone());
        }
        if self.last_events.len() == BOUNDARY_COUNT {
            self.last_events.pop_front();
        }
        self.last_events.push_back(event.clone());
    }

    /// Record a parse error
    pub fn record_error(&mut self) {
        self.errors += 1;
    }

    /// Print the final summary report
    pub fn print_report(&self) {
        println!("\n--- Summary ---");
        println!("Total messages   : {}", self.displayed_count);

        match (&self.first_timestamp, &self.last_timestamp) {
            (Some(first), Some(last)) => {
                println!(
                    "First ts received: {}",
                    first.format("%Y-%m-%dT%H:%M:%S%.9fZ")
                );
                println!(
                    "Last  ts received: {}",
                    last.format("%Y-%m-%dT%H:%M:%S%.9fZ")
                );
            }
            _ => {
                println!("No events processed");
            }
        }

        if !self.first_events.is_empty() {
            println!("\n--- First {} events ---", self.first_events.len());
            for event in &self.first_events {
                println!("{event}");
            }
        }

        if !self.last_events.is_empty() {
            println!("\n--- Last {} events ---", self.last_events.len());
            for event in &self.last_events {
                println!("{event}");
            }
        }
    }
}

impl Default for Summary {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use rust_decimal::Decimal;

    use super::*;
    use crate::model::{Action, Side};

    fn make_event(ts_recv: &str, order_id: u64) -> MarketDataEvent {
        MarketDataEvent {
            ts_recv: ts_recv.parse().unwrap(),
            ts_event: ts_recv.parse().unwrap(),
            rtype: 160,
            publisher_id: 101,
            instrument_id: 1000,
            action: Action::Add,
            side: Side::Bid,
            price: Some(Decimal::new(1_000_000_000, 9)),
            size: 10,
            channel_id: 1,
            order_id,
            flags: 0,
            ts_in_delta: 0,
            sequence: 0,
            symbol: "TEST".to_string(),
        }
    }

    fn make_clear_event(ts_recv: &str) -> MarketDataEvent {
        MarketDataEvent {
            ts_recv: ts_recv.parse().unwrap(),
            ts_event: ts_recv.parse().unwrap(),
            rtype: 160,
            publisher_id: 101,
            instrument_id: 1000,
            action: Action::Clear,
            side: Side::None,
            price: None,
            size: 0,
            channel_id: 1,
            order_id: 0,
            flags: 8,
            ts_in_delta: 0,
            sequence: 0,
            symbol: "TEST".to_string(),
        }
    }

    #[test]
    fn summary_empty() {
        let summary = Summary::new();
        assert_eq!(summary.total, 0);
        assert_eq!(summary.displayed_count, 0);
        assert_eq!(summary.errors, 0);
        assert!(summary.first_timestamp.is_none());
        assert!(summary.last_timestamp.is_none());
        assert!(summary.first_events.is_empty());
        assert!(summary.last_events.is_empty());
    }

    #[test]
    fn summary_single_event() {
        let mut summary = Summary::new();
        let event = make_event("2026-03-09T07:52:41.368148840Z", 1);
        summary.record(&event);

        assert_eq!(summary.total, 1);
        assert_eq!(summary.displayed_count, 1);
        assert!(summary.first_timestamp.is_some());
        assert_eq!(summary.first_timestamp, summary.last_timestamp);
        assert_eq!(summary.first_events.len(), 1);
        assert_eq!(summary.last_events.len(), 1);
    }

    #[test]
    fn summary_skips_clear_in_boundary_events() {
        let mut summary = Summary::new();
        let clear = make_clear_event("2026-03-09T07:52:41.368148840Z");
        let add = make_event("2026-03-09T07:52:42.000000000Z", 1);
        summary.record(&clear);
        summary.record(&add);

        assert_eq!(summary.total, 2);
        assert_eq!(summary.displayed_count, 1);
        assert_eq!(summary.first_events.len(), 1);
        assert_eq!(summary.first_events[0].action, Action::Add);
    }

    #[test]
    fn summary_tracks_first_and_last_10() {
        let mut summary = Summary::new();
        for i in 0..25u64 {
            let event = make_event("2026-03-09T07:52:41.368148840Z", i);
            summary.record(&event);
        }

        assert_eq!(summary.total, 25);
        assert_eq!(summary.first_events.len(), 10);
        assert_eq!(summary.last_events.len(), 10);

        // First events should be 0..10
        for (i, event) in summary.first_events.iter().enumerate() {
            assert_eq!(event.order_id, i as u64);
        }

        // Last events should be 15..25
        for (i, event) in summary.last_events.iter().enumerate() {
            assert_eq!(event.order_id, (15 + i) as u64);
        }
    }

    #[test]
    fn summary_fewer_than_10_events() {
        let mut summary = Summary::new();
        for i in 0..5u64 {
            let event = make_event("2026-03-09T07:52:41.368148840Z", i);
            summary.record(&event);
        }

        assert_eq!(summary.total, 5);
        assert_eq!(summary.first_events.len(), 5);
        assert_eq!(summary.last_events.len(), 5);
    }

    #[test]
    fn summary_tracks_timestamps() {
        let mut summary = Summary::new();
        let e1 = make_event("2026-03-09T08:00:00.000000000Z", 1);
        let e2 = make_event("2026-03-09T09:00:00.000000000Z", 2);
        let e3 = make_event("2026-03-09T10:00:00.000000000Z", 3);
        summary.record(&e1);
        summary.record(&e2);
        summary.record(&e3);

        assert_eq!(
            summary.first_timestamp.unwrap(),
            "2026-03-09T08:00:00Z".parse::<DateTime<Utc>>().unwrap()
        );
        assert_eq!(
            summary.last_timestamp.unwrap(),
            "2026-03-09T10:00:00Z".parse::<DateTime<Utc>>().unwrap()
        );
    }

    #[test]
    fn summary_error_counting() {
        let mut summary = Summary::new();
        summary.record_error();
        summary.record_error();
        assert_eq!(summary.errors, 2);
        assert_eq!(summary.total, 0);
    }

    #[test]
    fn summary_default_trait() {
        let summary = Summary::default();
        assert_eq!(summary.total, 0);
        assert_eq!(summary.displayed_count, 0);
    }
}
