use std::io::{BufRead, BufReader};
use std::path::PathBuf;

use back_tester::model::Action;
use back_tester::parser::parse_line;
use back_tester::processor::Summary;

fn fixture_path(name: &str) -> PathBuf {
    PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("tests")
        .join("fixtures")
        .join(name)
}

fn process_file(path: &std::path::Path) -> Summary {
    let file = std::fs::File::open(path).unwrap();
    let reader = BufReader::new(file);
    let mut summary = Summary::new();

    for line_result in reader.lines() {
        let line = line_result.unwrap();
        if line.trim().is_empty() {
            continue;
        }
        match parse_line(&line) {
            Ok(event) => summary.record(&event),
            Err(_) => summary.record_error(),
        }
    }

    summary
}

#[test]
fn process_mixed_fixture_end_to_end() {
    let summary = process_file(&fixture_path("sample_mixed.json"));
    assert_eq!(summary.total, 7);
    assert_eq!(summary.displayed_count, 6); // 1 Clear excluded
    assert_eq!(summary.errors, 0);
    assert!(summary.first_timestamp.is_some());
    assert!(summary.last_timestamp.is_some());

    // 6 non-Clear events in both first and last (since < 10)
    assert_eq!(summary.first_events.len(), 6);
    assert_eq!(summary.last_events.len(), 6);
}

#[test]
fn process_mixed_fixture_action_types() {
    let path = fixture_path("sample_mixed.json");
    let file = std::fs::File::open(&path).unwrap();
    let reader = BufReader::new(file);

    let events: Vec<_> = reader
        .lines()
        .filter_map(|l| l.ok())
        .filter(|l| !l.trim().is_empty())
        .map(|l| parse_line(&l).unwrap())
        .collect();

    assert_eq!(events.len(), 7);
    assert_eq!(events[0].action, Action::Clear);
    assert_eq!(events[1].action, Action::Add);
    assert_eq!(events[2].action, Action::Add);
    assert_eq!(events[3].action, Action::Cancel);
    assert_eq!(events[4].action, Action::Fill);
    assert_eq!(events[5].action, Action::Modify);
    assert_eq!(events[6].action, Action::Trade);
}

#[test]
fn process_mixed_fixture_timestamps_ordered() {
    let summary = process_file(&fixture_path("sample_mixed.json"));
    let first = summary.first_timestamp.unwrap();
    let last = summary.last_timestamp.unwrap();
    assert!(last >= first);
}

#[test]
fn process_single_record_file() {
    let summary = process_file(&fixture_path("single_record.json"));
    assert_eq!(summary.total, 1);
    assert_eq!(summary.errors, 0);
    assert_eq!(summary.first_events.len(), 1);
    assert_eq!(summary.last_events.len(), 1);
    assert_eq!(summary.first_timestamp, summary.last_timestamp);
}

#[test]
fn process_empty_file() {
    let summary = process_file(&fixture_path("empty.json"));
    assert_eq!(summary.total, 0);
    assert_eq!(summary.errors, 0);
    assert!(summary.first_timestamp.is_none());
    assert!(summary.last_timestamp.is_none());
    assert!(summary.first_events.is_empty());
    assert!(summary.last_events.is_empty());
}

#[test]
fn process_all_invalid_file() {
    let summary = process_file(&fixture_path("all_invalid.json"));
    assert_eq!(summary.total, 0);
    assert_eq!(summary.errors, 3);
    assert!(summary.first_timestamp.is_none());
}

#[test]
fn first_and_last_events_content_matches() {
    let summary = process_file(&fixture_path("sample_mixed.json"));

    // 6 non-Clear events (< 10), first and last should contain the same events
    assert_eq!(summary.first_events.len(), summary.last_events.len());
    for (first, last) in summary.first_events.iter().zip(summary.last_events.iter()) {
        assert_eq!(first.order_id, last.order_id);
        assert_eq!(first.action as u8, last.action as u8);
    }
    // Verify no Clear events in boundary lists
    for event in &summary.first_events {
        assert_ne!(event.action, Action::Clear);
    }
}

#[test]
fn summary_print_report_does_not_panic() {
    let summary = process_file(&fixture_path("sample_mixed.json"));
    // Just verify it doesn't panic
    summary.print_report();
}

#[test]
fn summary_print_report_empty_does_not_panic() {
    let summary = Summary::new();
    summary.print_report();
}
