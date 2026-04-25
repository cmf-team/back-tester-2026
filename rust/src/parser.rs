use std::fmt;

use chrono::{DateTime, Utc};
use rust_decimal::Decimal;
use serde::Deserialize;

use crate::model::{Action, MarketDataEvent, Side};

#[derive(Deserialize)]
struct RawRecord {
    ts_recv: String,
    hd: RawHeader,
    action: String,
    side: String,
    price: Option<String>,
    size: u32,
    channel_id: u16,
    order_id: String,
    flags: u8,
    ts_in_delta: i32,
    sequence: u32,
    symbol: String,
}

#[derive(Deserialize)]
struct RawHeader {
    ts_event: String,
    rtype: u8,
    publisher_id: u16,
    instrument_id: u32,
}

/// Errors that can occur when parsing a single NDJSON line
#[derive(Debug)]
pub enum ParseError {
    Json(serde_json::Error),
    Timestamp { field: &'static str, source: chrono::ParseError },
    InvalidAction(String),
    InvalidSide(String),
    InvalidPrice(String),
    InvalidOrderId(String),
    EmptyField(&'static str),
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            ParseError::Json(e) => write!(f, "JSON parse error: {e}"),
            ParseError::Timestamp { field, source } => {
                write!(f, "invalid timestamp in '{field}': {source}")
            }
            ParseError::InvalidAction(v) => write!(f, "invalid action: '{v}'"),
            ParseError::InvalidSide(v) => write!(f, "invalid side: '{v}'"),
            ParseError::InvalidPrice(v) => write!(f, "invalid price: '{v}'"),
            ParseError::InvalidOrderId(v) => write!(f, "invalid order_id: '{v}'"),
            ParseError::EmptyField(name) => write!(f, "empty field: '{name}'"),
        }
    }
}

impl std::error::Error for ParseError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match self {
            ParseError::Json(e) => Some(e),
            ParseError::Timestamp { source, .. } => Some(source),
            _ => None,
        }
    }
}

/// Parse a single NDJSON line into a MarketDataEvent
pub fn parse_line(line: &str) -> Result<MarketDataEvent, ParseError> {
    let raw: RawRecord = serde_json::from_str(line).map_err(ParseError::Json)?;

    let ts_recv = parse_timestamp(&raw.ts_recv, "ts_recv")?;
    let ts_event = parse_timestamp(&raw.hd.ts_event, "ts_event")?;

    let action = parse_action(&raw.action)?;
    let side = parse_side(&raw.side)?;
    let price = parse_price(raw.price.as_deref())?;
    let order_id = parse_order_id(&raw.order_id)?;

    Ok(MarketDataEvent {
        ts_recv,
        ts_event,
        rtype: raw.hd.rtype,
        publisher_id: raw.hd.publisher_id,
        instrument_id: raw.hd.instrument_id,
        action,
        side,
        price,
        size: raw.size,
        channel_id: raw.channel_id,
        order_id,
        flags: raw.flags,
        ts_in_delta: raw.ts_in_delta,
        sequence: raw.sequence,
        symbol: raw.symbol,
    })
}

fn parse_timestamp(s: &str, field: &'static str) -> Result<DateTime<Utc>, ParseError> {
    s.parse::<DateTime<Utc>>()
        .map_err(|e| ParseError::Timestamp { field, source: e })
}

fn parse_action(s: &str) -> Result<Action, ParseError> {
    let c = s
        .chars()
        .next()
        .ok_or(ParseError::EmptyField("action"))?;
    Action::from_char(c).map_err(|_| ParseError::InvalidAction(s.to_string()))
}

fn parse_side(s: &str) -> Result<Side, ParseError> {
    let c = s
        .chars()
        .next()
        .ok_or(ParseError::EmptyField("side"))?;
    Side::from_char(c).map_err(|_| ParseError::InvalidSide(s.to_string()))
}

fn parse_price(s: Option<&str>) -> Result<Option<Decimal>, ParseError> {
    match s {
        None => Ok(None),
        Some(v) => v
            .parse::<Decimal>()
            .map(Some)
            .map_err(|_| ParseError::InvalidPrice(v.to_string())),
    }
}

fn parse_order_id(s: &str) -> Result<u64, ParseError> {
    s.parse::<u64>()
        .map_err(|_| ParseError::InvalidOrderId(s.to_string()))
}

#[cfg(test)]
mod tests {
    use super::*;

    fn sample_add_line() -> &'static str {
        r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","side":"B","price":"0.021200000","size":20,"channel_id":79,"order_id":"10996414798222631105","flags":0,"ts_in_delta":2365,"sequence":52012,"symbol":"EUCO SI 20260710 PS EU P 1.1650 0"}"#
    }

    fn sample_clear_line() -> &'static str {
        r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"R","side":"N","price":null,"size":0,"channel_id":79,"order_id":"0","flags":8,"ts_in_delta":0,"sequence":0,"symbol":"EUCO SI 20260710 PS EU P 1.1650 0"}"#
    }

    fn sample_trade_line() -> &'static str {
        r#"{"ts_recv":"2026-03-10T12:21:35.336372858Z","hd":{"ts_event":"2026-03-10T12:21:35.334134034Z","rtype":160,"publisher_id":103,"instrument_id":34257},"action":"T","side":"N","price":"0.012280000","size":1096,"channel_id":79,"order_id":"0","flags":128,"ts_in_delta":1323,"sequence":114384,"symbol":"EUCO SI 20260410 PS EU P 1.1700 0"}"#
    }

    fn sample_fill_line() -> &'static str {
        r#"{"ts_recv":"2026-03-10T07:21:48.376976865Z","hd":{"ts_event":"2026-03-10T07:21:48.373755295Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"F","side":"B","price":"1.164080000","size":30,"channel_id":23,"order_id":"10996499332447372468","flags":0,"ts_in_delta":1248,"sequence":563729,"symbol":"FCEU SI 20260316 PS"}"#
    }

    fn sample_modify_line() -> &'static str {
        r#"{"ts_recv":"2026-03-10T07:21:57.429449992Z","hd":{"ts_event":"2026-03-10T07:21:57.429419129Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"M","side":"A","price":"1.164180000","size":21,"channel_id":23,"order_id":"1773127308373860304","flags":128,"ts_in_delta":1364,"sequence":564713,"symbol":"FCEU SI 20260316 PS"}"#
    }

    fn sample_cancel_line() -> &'static str {
        r#"{"ts_recv":"2026-03-10T08:02:05.388863569Z","hd":{"ts_event":"2026-03-10T08:02:05.388779725Z","rtype":160,"publisher_id":101,"instrument_id":34357},"action":"C","side":"B","price":"0.011730000","size":20,"channel_id":79,"order_id":"10996501761238634241","flags":0,"ts_in_delta":2234,"sequence":45005,"symbol":"EUCO SI 20260508 PS EU P 1.1675 0"}"#
    }

    #[test]
    fn parse_add_record() {
        let event = parse_line(sample_add_line()).unwrap();
        assert_eq!(event.action, Action::Add);
        assert_eq!(event.side, Side::Bid);
        assert_eq!(event.price, Some(Decimal::new(21_200_000, 9)));
        assert_eq!(event.size, 20);
        assert_eq!(event.order_id, 10996414798222631105);
        assert_eq!(event.instrument_id, 34513);
        assert_eq!(event.rtype, 160);
        assert_eq!(event.publisher_id, 101);
        assert_eq!(event.channel_id, 79);
        assert_eq!(event.flags, 0);
        assert_eq!(event.ts_in_delta, 2365);
        assert_eq!(event.sequence, 52012);
        assert_eq!(event.symbol, "EUCO SI 20260710 PS EU P 1.1650 0");
    }

    #[test]
    fn parse_clear_record_with_null_price() {
        let event = parse_line(sample_clear_line()).unwrap();
        assert_eq!(event.action, Action::Clear);
        assert_eq!(event.side, Side::None);
        assert_eq!(event.price, None);
        assert_eq!(event.size, 0);
        assert_eq!(event.order_id, 0);
        assert_eq!(event.flags, 8);
    }

    #[test]
    fn parse_trade_record() {
        let event = parse_line(sample_trade_line()).unwrap();
        assert_eq!(event.action, Action::Trade);
        assert_eq!(event.side, Side::None);
        assert_eq!(event.size, 1096);
        assert_eq!(event.flags, 128);
        assert!(event.price.is_some());
    }

    #[test]
    fn parse_fill_record() {
        let event = parse_line(sample_fill_line()).unwrap();
        assert_eq!(event.action, Action::Fill);
        assert_eq!(event.side, Side::Bid);
        assert_eq!(event.size, 30);
        assert_eq!(event.price, Some(Decimal::new(1_164_080_000, 9)));
    }

    #[test]
    fn parse_modify_record() {
        let event = parse_line(sample_modify_line()).unwrap();
        assert_eq!(event.action, Action::Modify);
        assert_eq!(event.side, Side::Ask);
        assert_eq!(event.size, 21);
        assert_eq!(event.flags, 128);
    }

    #[test]
    fn parse_cancel_record() {
        let event = parse_line(sample_cancel_line()).unwrap();
        assert_eq!(event.action, Action::Cancel);
        assert_eq!(event.side, Side::Bid);
        assert_eq!(event.size, 20);
    }

    #[test]
    fn error_on_malformed_json() {
        let result = parse_line("this is not json");
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), ParseError::Json(_)));
    }

    #[test]
    fn error_on_missing_field() {
        let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513}}"#;
        let result = parse_line(line);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), ParseError::Json(_)));
    }

    #[test]
    fn error_on_invalid_action() {
        let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"X","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"TEST"}"#;
        let result = parse_line(line);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), ParseError::InvalidAction(_)));
    }

    #[test]
    fn error_on_invalid_side() {
        let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","side":"Z","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"TEST"}"#;
        let result = parse_line(line);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), ParseError::InvalidSide(_)));
    }

    #[test]
    fn error_on_invalid_timestamp() {
        let line = r#"{"ts_recv":"not-a-timestamp","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"TEST"}"#;
        let result = parse_line(line);
        assert!(result.is_err());
        assert!(matches!(
            result.unwrap_err(),
            ParseError::Timestamp { field: "ts_recv", .. }
        ));
    }

    #[test]
    fn error_on_invalid_price() {
        let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","side":"B","price":"not_a_number","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"TEST"}"#;
        let result = parse_line(line);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), ParseError::InvalidPrice(_)));
    }

    #[test]
    fn error_on_invalid_order_id() {
        let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"not_a_number","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"TEST"}"#;
        let result = parse_line(line);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), ParseError::InvalidOrderId(_)));
    }

    #[test]
    fn sentinel_order_id_zero() {
        let event = parse_line(sample_clear_line()).unwrap();
        assert_eq!(event.order_id, 0);
    }

    #[test]
    fn large_order_id() {
        let event = parse_line(sample_add_line()).unwrap();
        assert_eq!(event.order_id, 10996414798222631105);
        assert!(event.order_id > u32::MAX as u64);
    }

    #[test]
    fn price_decimal_precision() {
        let event = parse_line(sample_add_line()).unwrap();
        let price = event.price.unwrap();
        assert_eq!(price.to_string(), "0.021200000");
    }

    #[test]
    fn timestamps_parsed_correctly() {
        let event = parse_line(sample_add_line()).unwrap();
        assert_eq!(
            event.ts_recv.to_rfc3339_opts(chrono::SecondsFormat::Nanos, true),
            "2026-03-09T07:52:41.368148840Z"
        );
        assert_eq!(
            event.ts_event.to_rfc3339_opts(chrono::SecondsFormat::Nanos, true),
            "2026-03-09T07:52:41.367824437Z"
        );
    }

    #[test]
    fn parse_error_display() {
        let err = ParseError::InvalidAction("X".to_string());
        assert_eq!(err.to_string(), "invalid action: 'X'");

        let err = ParseError::EmptyField("action");
        assert_eq!(err.to_string(), "empty field: 'action'");
    }
}
