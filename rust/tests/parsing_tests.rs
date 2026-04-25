use back_tester::model::{Action, Side};
use back_tester::parser::{parse_line, ParseError};
use rust_decimal::Decimal;

// --- Valid record parsing for each action type ---

#[test]
fn parse_add_record() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"A","side":"B","price":"0.021200000","size":20,"channel_id":79,"order_id":"10996414798222631105","flags":0,"ts_in_delta":2365,"sequence":52012,"symbol":"EUCO SI 20260710 PS EU P 1.1650 0"}"#;
    let event = parse_line(line).unwrap();
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
fn parse_clear_record_null_price() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":34513},"action":"R","side":"N","price":null,"size":0,"channel_id":79,"order_id":"0","flags":8,"ts_in_delta":0,"sequence":0,"symbol":"TEST"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.action, Action::Clear);
    assert_eq!(event.side, Side::None);
    assert_eq!(event.price, None);
    assert_eq!(event.size, 0);
    assert_eq!(event.order_id, 0);
    assert_eq!(event.flags, 8);
}

#[test]
fn parse_trade_record() {
    let line = r#"{"ts_recv":"2026-03-10T12:21:35.336372858Z","hd":{"ts_event":"2026-03-10T12:21:35.334134034Z","rtype":160,"publisher_id":103,"instrument_id":34257},"action":"T","side":"N","price":"0.012280000","size":1096,"channel_id":79,"order_id":"0","flags":128,"ts_in_delta":1323,"sequence":114384,"symbol":"TEST"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.action, Action::Trade);
    assert_eq!(event.side, Side::None);
    assert_eq!(event.price, Some(Decimal::new(12_280_000, 9)));
    assert_eq!(event.size, 1096);
    assert_eq!(event.flags, 128);
}

#[test]
fn parse_fill_record() {
    let line = r#"{"ts_recv":"2026-03-10T07:21:48.376976865Z","hd":{"ts_event":"2026-03-10T07:21:48.373755295Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"F","side":"B","price":"1.164080000","size":30,"channel_id":23,"order_id":"10996499332447372468","flags":0,"ts_in_delta":1248,"sequence":563729,"symbol":"FCEU SI 20260316 PS"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.action, Action::Fill);
    assert_eq!(event.side, Side::Bid);
    assert_eq!(event.price, Some(Decimal::new(1_164_080_000, 9)));
    assert_eq!(event.size, 30);
}

#[test]
fn parse_modify_record() {
    let line = r#"{"ts_recv":"2026-03-10T07:21:57.429449992Z","hd":{"ts_event":"2026-03-10T07:21:57.429419129Z","rtype":160,"publisher_id":101,"instrument_id":442},"action":"M","side":"A","price":"1.164180000","size":21,"channel_id":23,"order_id":"1773127308373860304","flags":128,"ts_in_delta":1364,"sequence":564713,"symbol":"FCEU SI 20260316 PS"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.action, Action::Modify);
    assert_eq!(event.side, Side::Ask);
    assert_eq!(event.size, 21);
    assert_eq!(event.flags, 128);
}

#[test]
fn parse_cancel_record() {
    let line = r#"{"ts_recv":"2026-03-10T08:02:05.388863569Z","hd":{"ts_event":"2026-03-10T08:02:05.388779725Z","rtype":160,"publisher_id":101,"instrument_id":34357},"action":"C","side":"B","price":"0.011730000","size":20,"channel_id":79,"order_id":"10996501761238634241","flags":0,"ts_in_delta":2234,"sequence":45005,"symbol":"TEST"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.action, Action::Cancel);
    assert_eq!(event.side, Side::Bid);
    assert_eq!(event.size, 20);
}

#[test]
fn parse_none_action() {
    let line = r#"{"ts_recv":"2026-03-10T08:00:00.000000000Z","hd":{"ts_event":"2026-03-10T08:00:00.000000000Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"N","side":"N","price":null,"size":0,"channel_id":1,"order_id":"0","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"TEST"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.action, Action::None);
}

// --- Flag combinations ---

#[test]
fn parse_flags_zero() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.flags, 0);
}

#[test]
fn parse_flags_bad_ts_recv() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"R","side":"N","price":null,"size":0,"channel_id":1,"order_id":"0","flags":8,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.flags, 8);
}

#[test]
fn parse_flags_last() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"A","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":128,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.flags, 128);
}

// --- Error cases ---

#[test]
fn error_malformed_json() {
    let result = parse_line("this is not json at all");
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), ParseError::Json(_)));
}

#[test]
fn error_incomplete_json() {
    let result = parse_line(r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z"}"#);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), ParseError::Json(_)));
}

#[test]
fn error_invalid_action() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"X","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let result = parse_line(line);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), ParseError::InvalidAction(_)));
}

#[test]
fn error_invalid_side() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"Z","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let result = parse_line(line);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), ParseError::InvalidSide(_)));
}

#[test]
fn error_invalid_timestamp_ts_recv() {
    let line = r#"{"ts_recv":"not-a-timestamp","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let result = parse_line(line);
    assert!(result.is_err());
    match result.unwrap_err() {
        ParseError::Timestamp { field, .. } => assert_eq!(field, "ts_recv"),
        other => panic!("expected Timestamp error, got: {other}"),
    }
}

#[test]
fn error_invalid_timestamp_ts_event() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"bad-time","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let result = parse_line(line);
    assert!(result.is_err());
    match result.unwrap_err() {
        ParseError::Timestamp { field, .. } => assert_eq!(field, "ts_event"),
        other => panic!("expected Timestamp error, got: {other}"),
    }
}

#[test]
fn error_invalid_price() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"abc","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let result = parse_line(line);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), ParseError::InvalidPrice(_)));
}

#[test]
fn error_invalid_order_id() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"not_a_number","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let result = parse_line(line);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), ParseError::InvalidOrderId(_)));
}

#[test]
fn error_negative_order_id() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"-1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let result = parse_line(line);
    assert!(result.is_err());
    assert!(matches!(result.unwrap_err(), ParseError::InvalidOrderId(_)));
}

// --- Edge cases ---

#[test]
fn sentinel_order_id_zero() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"R","side":"N","price":null,"size":0,"channel_id":1,"order_id":"0","flags":8,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.order_id, 0);
}

#[test]
fn large_order_id_u64_range() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"18446744073709551615","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.order_id, u64::MAX);
}

#[test]
fn price_precision_nine_decimals() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"0.000000001","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.price, Some(Decimal::new(1, 9)));
}

#[test]
fn price_whole_number() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"100.000000000","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let event = parse_line(line).unwrap();
    let price = event.price.unwrap();
    assert_eq!(price, Decimal::new(100_000_000_000, 9));
}

#[test]
fn ask_side_with_add_action() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"A","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":128,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.action, Action::Add);
    assert_eq!(event.side, Side::Ask);
}

#[test]
fn negative_ts_in_delta() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"A","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":-500,"sequence":0,"symbol":"T"}"#;
    let event = parse_line(line).unwrap();
    assert_eq!(event.ts_in_delta, -500);
}

// --- Parse error Display ---

#[test]
fn parse_error_display_json() {
    let err = parse_line("bad json").unwrap_err();
    let msg = err.to_string();
    assert!(msg.contains("JSON parse error"));
}

#[test]
fn parse_error_display_action() {
    let line = r#"{"ts_recv":"2026-03-09T07:52:41.368148840Z","hd":{"ts_event":"2026-03-09T07:52:41.367824437Z","rtype":160,"publisher_id":101,"instrument_id":1},"action":"X","side":"B","price":"1.0","size":1,"channel_id":1,"order_id":"1","flags":0,"ts_in_delta":0,"sequence":0,"symbol":"T"}"#;
    let err = parse_line(line).unwrap_err();
    assert!(err.to_string().contains("invalid action"));
}

#[test]
fn parse_error_source_chain() {
    let err = parse_line("bad json").unwrap_err();
    assert!(std::error::Error::source(&err).is_some());
}
