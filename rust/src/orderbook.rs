use std::collections::{BTreeMap, HashMap};

use rust_decimal::Decimal;

use crate::model::{Action, MarketDataEvent, Side};

#[derive(Debug, Clone)]
#[allow(dead_code)]
struct Order {
    order_id: u64,
    side: Side,
    price: Decimal,
    size: u32,
}

/// Per-instrument Limit Order Book maintaining L3 orders and L2 aggregated price levels
pub struct LimitOrderBook {
    instrument_id: u32,
    symbol: Option<String>,
    orders: HashMap<u64, Order>,
    bids: BTreeMap<Decimal, u32>,
    asks: BTreeMap<Decimal, u32>,
}

impl LimitOrderBook {
    pub fn new(instrument_id: u32) -> Self {
        Self {
            instrument_id,
            symbol: None,
            orders: HashMap::new(),
            bids: BTreeMap::new(),
            asks: BTreeMap::new(),
        }
    }

    /// Apply a market data event to update the book
    pub fn apply(&mut self, event: &MarketDataEvent) {
        if self.symbol.is_none() {
            self.symbol = Some(event.symbol.clone());
        }

        match event.action {
            Action::Add => self.handle_add(event),
            Action::Cancel => self.handle_cancel(event),
            Action::Modify => self.handle_modify(event),
            Action::Clear => self.handle_clear(),
            Action::Trade | Action::Fill | Action::None => {}
        }
    }

    fn handle_add(&mut self, event: &MarketDataEvent) {
        let price = match event.price {
            Some(p) => p,
            None => return,
        };
        if event.side == Side::None {
            return;
        }

        let order = Order {
            order_id: event.order_id,
            side: event.side,
            price,
            size: event.size,
        };

        self.add_to_level(event.side, price, event.size);
        self.orders.insert(event.order_id, order);
    }

    fn handle_cancel(&mut self, event: &MarketDataEvent) {
        if let Some(order) = self.orders.remove(&event.order_id) {
            self.remove_from_level(order.side, order.price, order.size);
        }
    }

    fn handle_modify(&mut self, event: &MarketDataEvent) {
        let price = match event.price {
            Some(p) => p,
            None => return,
        };
        if event.side == Side::None {
            return;
        }

        if let Some(old_order) = self.orders.remove(&event.order_id) {
            self.remove_from_level(old_order.side, old_order.price, old_order.size);
        }

        let order = Order {
            order_id: event.order_id,
            side: event.side,
            price,
            size: event.size,
        };
        self.add_to_level(event.side, price, event.size);
        self.orders.insert(event.order_id, order);
    }

    fn handle_clear(&mut self) {
        self.orders.clear();
        self.bids.clear();
        self.asks.clear();
    }

    fn add_to_level(&mut self, side: Side, price: Decimal, size: u32) {
        let levels = match side {
            Side::Bid => &mut self.bids,
            Side::Ask => &mut self.asks,
            Side::None => return,
        };
        *levels.entry(price).or_insert(0) += size;
    }

    fn remove_from_level(&mut self, side: Side, price: Decimal, size: u32) {
        let levels = match side {
            Side::Bid => &mut self.bids,
            Side::Ask => &mut self.asks,
            Side::None => return,
        };
        if let Some(level_size) = levels.get_mut(&price) {
            if *level_size <= size {
                levels.remove(&price);
            } else {
                *level_size -= size;
            }
        }
    }

    /// Returns the best (highest) bid price and aggregated size
    pub fn best_bid(&self) -> Option<(Decimal, u32)> {
        self.bids.iter().next_back().map(|(p, s)| (*p, *s))
    }

    /// Returns the best (lowest) ask price and aggregated size
    pub fn best_ask(&self) -> Option<(Decimal, u32)> {
        self.asks.iter().next().map(|(p, s)| (*p, *s))
    }

    /// Returns the aggregated volume at a given price level and side
    pub fn volume_at_price(&self, side: Side, price: &Decimal) -> u32 {
        let levels = match side {
            Side::Bid => &self.bids,
            Side::Ask => &self.asks,
            Side::None => return 0,
        };
        levels.get(price).copied().unwrap_or(0)
    }

    /// Print a snapshot showing the top `depth` levels on each side
    pub fn print_snapshot(&self, depth: usize) {
        let symbol = self.symbol.as_deref().unwrap_or("unknown");
        println!(
            "\n=== LOB Snapshot: instrument_id={} symbol={} ===",
            self.instrument_id, symbol
        );
        println!(
            "Orders: {} | Bid levels: {} | Ask levels: {}",
            self.orders.len(),
            self.bids.len(),
            self.asks.len()
        );

        let ask_levels: Vec<_> = self.asks.iter().take(depth).collect();
        println!(
            "  {:>12}  {:>8}  {:>8}  {:>12}",
            "Ask Price", "Ask Size", "Bid Size", "Bid Price"
        );
        println!("  {}", "-".repeat(46));

        let bid_levels: Vec<_> = self.bids.iter().rev().take(depth).collect();

        let max_rows = ask_levels.len().max(bid_levels.len());
        for i in 0..max_rows {
            let ask_str = if i < ask_levels.len() {
                format!(
                    "  {:>12}  {:>8}",
                    ask_levels[i].0.normalize(),
                    ask_levels[i].1
                )
            } else {
                format!("  {:>12}  {:>8}", "", "")
            };
            let bid_str = if i < bid_levels.len() {
                format!(
                    "  {:>8}  {:>12}",
                    bid_levels[i].1,
                    bid_levels[i].0.normalize()
                )
            } else {
                String::new()
            };
            println!("{}{}", ask_str, bid_str);
        }

        match (self.best_bid(), self.best_ask()) {
            (Some((bp, _)), Some((ap, _))) => {
                let spread = ap - bp;
                println!("  Spread: {}", spread.normalize());
            }
            _ => println!("  Spread: N/A"),
        }
    }

    pub fn instrument_id(&self) -> u32 {
        self.instrument_id
    }

    pub fn symbol(&self) -> Option<&str> {
        self.symbol.as_deref()
    }

    pub fn bid_levels(&self) -> usize {
        self.bids.len()
    }

    pub fn ask_levels(&self) -> usize {
        self.asks.len()
    }

    pub fn order_count(&self) -> usize {
        self.orders.len()
    }
}
