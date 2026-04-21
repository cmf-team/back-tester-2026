use std::env;
use std::fs::File;
use std::io::{BufRead, BufReader};
use std::process;

use back_tester::parser::parse_line;
use back_tester::processor::{process_market_data_event, Summary};

fn main() {
    let args: Vec<String> = env::args().collect();
    if args.len() != 2 {
        eprintln!("Usage: {} <path-to-ndjson-file>", args[0]);
        process::exit(1);
    }

    let file_path = &args[1];
    let file = match File::open(file_path) {
        Ok(f) => f,
        Err(e) => {
            eprintln!("Error opening file '{file_path}': {e}");
            process::exit(1);
        }
    };

    let reader = BufReader::new(file);
    let mut summary = Summary::new();

    for (line_num, line_result) in reader.lines().enumerate() {
        let line = match line_result {
            Ok(l) => l,
            Err(e) => {
                eprintln!("Error reading line {}: {e}", line_num + 1);
                summary.record_error();
                continue;
            }
        };

        if line.trim().is_empty() {
            continue;
        }

        match parse_line(&line) {
            Ok(event) => {
                process_market_data_event(&event, &summary);
                summary.record(&event);
            }
            Err(e) => {
                eprintln!("Error parsing line {}: {e}", line_num + 1);
                summary.record_error();
            }
        }
    }

    summary.print_report();
}
