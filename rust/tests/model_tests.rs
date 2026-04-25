use back_tester::model::{flags, Action, Side};

// --- Action enum tests ---

#[test]
fn action_from_char_all_variants() {
    assert_eq!(Action::from_char('A').unwrap(), Action::Add);
    assert_eq!(Action::from_char('M').unwrap(), Action::Modify);
    assert_eq!(Action::from_char('C').unwrap(), Action::Cancel);
    assert_eq!(Action::from_char('R').unwrap(), Action::Clear);
    assert_eq!(Action::from_char('T').unwrap(), Action::Trade);
    assert_eq!(Action::from_char('F').unwrap(), Action::Fill);
    assert_eq!(Action::from_char('N').unwrap(), Action::None);
}

#[test]
fn action_from_char_invalid() {
    assert!(Action::from_char('X').is_err());
    assert!(Action::from_char('a').is_err());
    assert!(Action::from_char('0').is_err());
    assert!(Action::from_char(' ').is_err());
}

#[test]
fn action_roundtrip_char() {
    let actions = [
        Action::Add,
        Action::Modify,
        Action::Cancel,
        Action::Clear,
        Action::Trade,
        Action::Fill,
        Action::None,
    ];
    for action in &actions {
        let c = action.as_char();
        assert_eq!(Action::from_char(c).unwrap(), *action);
    }
}

#[test]
fn action_display() {
    assert_eq!(format!("{}", Action::Add), "Add");
    assert_eq!(format!("{}", Action::Modify), "Modify");
    assert_eq!(format!("{}", Action::Cancel), "Cancel");
    assert_eq!(format!("{}", Action::Clear), "Clear");
    assert_eq!(format!("{}", Action::Trade), "Trade");
    assert_eq!(format!("{}", Action::Fill), "Fill");
    assert_eq!(format!("{}", Action::None), "None");
}

// --- Side enum tests ---

#[test]
fn side_from_char_all_variants() {
    assert_eq!(Side::from_char('A').unwrap(), Side::Ask);
    assert_eq!(Side::from_char('B').unwrap(), Side::Bid);
    assert_eq!(Side::from_char('N').unwrap(), Side::None);
}

#[test]
fn side_from_char_invalid() {
    assert!(Side::from_char('X').is_err());
    assert!(Side::from_char('b').is_err());
    assert!(Side::from_char('S').is_err());
}

#[test]
fn side_roundtrip_char() {
    let sides = [Side::Ask, Side::Bid, Side::None];
    for side in &sides {
        let c = side.as_char();
        assert_eq!(Side::from_char(c).unwrap(), *side);
    }
}

#[test]
fn side_display() {
    assert_eq!(format!("{}", Side::Ask), "Sell");
    assert_eq!(format!("{}", Side::Bid), "Buy");
    assert_eq!(format!("{}", Side::None), "None");
}

// --- Flags tests ---

#[test]
fn flag_constants_correct_values() {
    assert_eq!(flags::F_LAST, 128);
    assert_eq!(flags::F_TOB, 64);
    assert_eq!(flags::F_SNAPSHOT, 32);
    assert_eq!(flags::F_MBP, 16);
    assert_eq!(flags::F_BAD_TS_RECV, 8);
    assert_eq!(flags::F_MAYBE_BAD_BOOK, 4);
    assert_eq!(flags::F_PUBLISHER_SPECIFIC, 2);
}

#[test]
fn has_flag_individual() {
    assert!(flags::has_flag(128, flags::F_LAST));
    assert!(!flags::has_flag(128, flags::F_TOB));
    assert!(flags::has_flag(8, flags::F_BAD_TS_RECV));
    assert!(!flags::has_flag(0, flags::F_LAST));
}

#[test]
fn has_flag_combined() {
    let combined = flags::F_LAST | flags::F_BAD_TS_RECV; // 128 + 8 = 136
    assert!(flags::has_flag(combined, flags::F_LAST));
    assert!(flags::has_flag(combined, flags::F_BAD_TS_RECV));
    assert!(!flags::has_flag(combined, flags::F_TOB));
}

#[test]
fn format_flags_zero() {
    assert_eq!(flags::format_flags(0), "0");
}

#[test]
fn format_flags_single() {
    assert_eq!(flags::format_flags(flags::F_LAST), "LAST");
    assert_eq!(flags::format_flags(flags::F_BAD_TS_RECV), "BAD_TS_RECV");
}

#[test]
fn format_flags_combined() {
    let combined = flags::F_LAST | flags::F_BAD_TS_RECV;
    let formatted = flags::format_flags(combined);
    assert!(formatted.contains("LAST"));
    assert!(formatted.contains("BAD_TS_RECV"));
    assert!(formatted.contains('|'));
}

// --- ParseEnumError tests ---

#[test]
fn parse_enum_error_display() {
    let err = Action::from_char('Z').unwrap_err();
    assert_eq!(err.to_string(), "invalid Action value: 'Z'");

    let err = Side::from_char('X').unwrap_err();
    assert_eq!(err.to_string(), "invalid Side value: 'X'");
}

// --- MarketDataEvent Display test ---

#[test]
fn market_data_event_display() {
    use chrono::Utc;
    use rust_decimal::Decimal;

    let event = back_tester::model::MarketDataEvent {
        ts_recv: "2026-03-09T07:52:41.368148840Z"
            .parse::<chrono::DateTime<Utc>>()
            .unwrap(),
        ts_event: "2026-03-09T07:52:41.367824437Z"
            .parse::<chrono::DateTime<Utc>>()
            .unwrap(),
        rtype: 160,
        publisher_id: 101,
        instrument_id: 34513,
        action: Action::Add,
        side: Side::Bid,
        price: Some(Decimal::new(21_200_000, 9)),
        size: 20,
        channel_id: 79,
        order_id: 10996414798222631105,
        flags: 0,
        ts_in_delta: 2365,
        sequence: 52012,
        symbol: "EUCO SI 20260710 PS EU P 1.1650 0".to_string(),
    };

    let display = format!("{event}");
    assert!(display.contains("2026-03-09T07:52:41.367824437Z"));
    assert!(display.contains("10996414798222631105"));
    assert!(display.contains("Buy"));
    assert!(display.contains("0.0212"));
    assert!(display.contains("20"));
    assert!(display.contains("Add"));
}

#[test]
fn market_data_event_display_null_price() {
    use chrono::Utc;

    let event = back_tester::model::MarketDataEvent {
        ts_recv: "2026-03-09T07:52:41.368148840Z"
            .parse::<chrono::DateTime<Utc>>()
            .unwrap(),
        ts_event: "2026-03-09T07:52:41.367824437Z"
            .parse::<chrono::DateTime<Utc>>()
            .unwrap(),
        rtype: 160,
        publisher_id: 101,
        instrument_id: 34513,
        action: Action::Clear,
        side: Side::None,
        price: None,
        size: 0,
        channel_id: 79,
        order_id: 0,
        flags: 8,
        ts_in_delta: 0,
        sequence: 0,
        symbol: "TEST".to_string(),
    };

    let display = format!("{event}");
    assert!(display.contains("null"));
    assert!(display.contains("Clear"));
    assert!(display.contains("None"));
}
