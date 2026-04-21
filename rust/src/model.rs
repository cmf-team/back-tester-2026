use std::fmt;

use chrono::{DateTime, Utc};
use rust_decimal::Decimal;

/// Order action type from Databento MBO feed
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Action {
    /// Insert a new order into the book
    Add,
    /// Change an order's price and/or size
    Modify,
    /// Fully or partially cancel an order from the book
    Cancel,
    /// Remove all resting orders for the instrument
    Clear,
    /// An aggressing order traded (does not affect the book)
    Trade,
    /// A resting order was filled (does not affect the book)
    Fill,
    /// No action: does not affect the book, but may carry flags or other information
    None,
}

impl Action {
    pub fn from_char(c: char) -> Result<Self, ParseEnumError> {
        match c {
            'A' => Ok(Action::Add),
            'M' => Ok(Action::Modify),
            'C' => Ok(Action::Cancel),
            'R' => Ok(Action::Clear),
            'T' => Ok(Action::Trade),
            'F' => Ok(Action::Fill),
            'N' => Ok(Action::None),
            _ => Err(ParseEnumError {
                type_name: "Action",
                value: c.to_string(),
            }),
        }
    }

    pub fn as_char(&self) -> char {
        match self {
            Action::Add => 'A',
            Action::Modify => 'M',
            Action::Cancel => 'C',
            Action::Clear => 'R',
            Action::Trade => 'T',
            Action::Fill => 'F',
            Action::None => 'N',
        }
    }
}

impl fmt::Display for Action {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let label = match self {
            Action::Add => "Add",
            Action::Modify => "Modify",
            Action::Cancel => "Cancel",
            Action::Clear => "Clear",
            Action::Trade => "Trade",
            Action::Fill => "Fill",
            Action::None => "None",
        };
        write!(f, "{label}")
    }
}

/// Order side from Databento MBO feed
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Side {
    /// Ask / sell side
    Ask,
    /// Bid / buy side
    Bid,
    /// No side specified
    None,
}

impl Side {
    pub fn from_char(c: char) -> Result<Self, ParseEnumError> {
        match c {
            'A' => Ok(Side::Ask),
            'B' => Ok(Side::Bid),
            'N' => Ok(Side::None),
            _ => Err(ParseEnumError {
                type_name: "Side",
                value: c.to_string(),
            }),
        }
    }

    pub fn as_char(&self) -> char {
        match self {
            Side::Ask => 'A',
            Side::Bid => 'B',
            Side::None => 'N',
        }
    }
}

impl fmt::Display for Side {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let label = match self {
            Side::Ask => "Sell",
            Side::Bid => "Buy",
            Side::None => "None",
        };
        write!(f, "{label}")
    }
}

/// Error when parsing an enum variant from a character
#[derive(Debug, Clone)]
pub struct ParseEnumError {
    pub type_name: &'static str,
    pub value: String,
}

impl fmt::Display for ParseEnumError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(
            f,
            "invalid {} value: '{}'",
            self.type_name, self.value
        )
    }
}

impl std::error::Error for ParseEnumError {}

/// Databento MBO message flag bits
pub mod flags {
    /// Marks the last record in a single event for a given instrument_id
    pub const F_LAST: u8 = 1 << 7;
    /// Top-of-book message, not an individual order
    pub const F_TOB: u8 = 1 << 6;
    /// Message sourced from a replay, such as a snapshot server
    pub const F_SNAPSHOT: u8 = 1 << 5;
    /// Aggregated price level message, not an individual order
    pub const F_MBP: u8 = 1 << 4;
    /// The ts_recv value is inaccurate due to clock issues or packet reordering
    pub const F_BAD_TS_RECV: u8 = 1 << 3;
    /// An unrecoverable gap was detected in the channel
    pub const F_MAYBE_BAD_BOOK: u8 = 1 << 2;
    /// Semantics depend on the publisher_id
    pub const F_PUBLISHER_SPECIFIC: u8 = 1 << 1;

    /// Check whether a specific flag bit is set
    pub fn has_flag(flags: u8, flag: u8) -> bool {
        flags & flag != 0
    }

    /// Format flag bits as a human-readable string
    pub fn format_flags(flags: u8) -> String {
        if flags == 0 {
            return "0".to_string();
        }
        let mut parts = Vec::new();
        if has_flag(flags, F_LAST) {
            parts.push("LAST");
        }
        if has_flag(flags, F_TOB) {
            parts.push("TOB");
        }
        if has_flag(flags, F_SNAPSHOT) {
            parts.push("SNAPSHOT");
        }
        if has_flag(flags, F_MBP) {
            parts.push("MBP");
        }
        if has_flag(flags, F_BAD_TS_RECV) {
            parts.push("BAD_TS_RECV");
        }
        if has_flag(flags, F_MAYBE_BAD_BOOK) {
            parts.push("MAYBE_BAD_BOOK");
        }
        if has_flag(flags, F_PUBLISHER_SPECIFIC) {
            parts.push("PUBLISHER_SPECIFIC");
        }
        parts.join("|")
    }
}

/// A single market data event parsed from a Databento NDJSON L3 MBO record
#[derive(Debug, Clone)]
pub struct MarketDataEvent {
    /// Databento receive timestamp (UTC)
    pub ts_recv: DateTime<Utc>,
    /// Exchange event timestamp (UTC)
    pub ts_event: DateTime<Utc>,
    /// Record type (160 = MBO)
    pub rtype: u8,
    /// Publisher identifier
    pub publisher_id: u16,
    /// Instrument identifier (unique within a day)
    pub instrument_id: u32,
    /// Order action type
    pub action: Action,
    /// Order side
    pub side: Side,
    /// Price in fixed-precision decimal (None for null prices, e.g., Clear actions)
    pub price: Option<Decimal>,
    /// Order quantity
    pub size: u32,
    /// Channel identifier
    pub channel_id: u16,
    /// Order identifier (0 = sentinel)
    pub order_id: u64,
    /// Message flags (bitfield)
    pub flags: u8,
    /// Nanoseconds between ts_recv and publisher sending timestamp
    pub ts_in_delta: i32,
    /// Sequence number
    pub sequence: u32,
    /// Instrument symbol
    pub symbol: String,
}

impl fmt::Display for MarketDataEvent {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let price_str = match &self.price {
            Some(p) => p.normalize().to_string(),
            Option::None => "null".to_string(),
        };
        write!(
            f,
            "ts_event, usual={} oid={} side={} price={} size={} action={}",
            self.ts_event.format("%Y-%m-%dT%H:%M:%S%.9fZ"),
            self.order_id,
            self.side,
            price_str,
            self.size,
            self.action,
        )
    }
}
