use std::env;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::process;
use std::time::Instant;

use back_tester::dispatcher::Dispatcher;
use back_tester::parser::parse_line;

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() < 2 || args.len() > 3 {
        eprintln!(
            "Usage: {} <path-to-ndjson-file> [snapshot-interval]",
            args[0]
        );
        process::exit(1);
    }

    let file_path = &args[1];
    let snapshot_interval: u64 = if args.len() == 3 {
        args[2].parse().unwrap_or_else(|_| {
            eprintln!("Invalid snapshot interval: '{}'", args[2]);
            process::exit(1);
        })
    } else {
        500_000
    };

    let file = match File::open(file_path) {
        Ok(f) => f,
        Err(e) => {
            eprintln!("Error opening file '{file_path}': {e}");
            process::exit(1);
        }
    };

    let reader = BufReader::new(file);
    let mut dispatcher = Dispatcher::new(snapshot_interval);
    let mut errors = 0u64;

    let start = Instant::now();

    for (line_num, line_result) in reader.lines().enumerate() {
        let line = match line_result {
            Ok(l) => l,
            Err(e) => {
                eprintln!("Error reading line {}: {e}", line_num + 1);
                errors += 1;
                continue;
            }
        };

        if line.trim().is_empty() {
            continue;
        }

        match parse_line(&line) {
            Ok(event) => {
                dispatcher.dispatch(&event);
            }
            Err(e) => {
                eprintln!("Error parsing line {}: {e}", line_num + 1);
                errors += 1;
            }
        }
    }

    let elapsed = start.elapsed();

    dispatcher.print_final_summary();

    let total = dispatcher.total_events();
    let secs = elapsed.as_secs_f64();
    let rate = if secs > 0.0 {
        total as f64 / secs
    } else {
        0.0
    };

    println!("\n--- Performance ---");
    println!("Total events : {}", total);
    println!("Parse errors : {}", errors);
    println!("Instruments  : {}", dispatcher.book_count());
    println!("Elapsed      : {:.4} s", secs);
    println!("Throughput   : {:.0} events/sec", rate);
}
