README.md
# CMF Backtesting Engine – Task 1 (Data Ingestion)

## Overview

This project implements the basic data ingestion layer for an event-driven backtester.

The program reads a daily NDJSON (JSON Lines) file with market data messages, parses each line into a structured `MarketDataEvent`, and computes summary statistics.

---

## Features

- Read NDJSON files line-by-line
- Parse JSON messages into `MarketDataEvent`
- Handle optional fields (e.g. `price = null`)
- Compute summary statistics:
  - total number of messages
  - first timestamp
  - last timestamp
- Basic error handling (skip invalid lines)
- Unit tests for parser and summary

---

## Project Structure


src/
common/
MarketDataEvent.h
JsonLineParser.cpp
Summary.cpp
main/
main.cpp

test/
test_parser.cpp
test_summary.cpp


---

## Build (MSYS2 / MinGW)

```bash
cmake -B build -S . -G "MinGW Makefiles"
cmake --build build
Run
./build/back-tester.exe data/extracted/xeur-eobi-20260309.mbo.json

Output:

=== SUMMARY ===
Total messages: ...
First timestamp: ...
Last timestamp: ...
Tests

Run parser test:

./build/test_parser.exe

Run summary test:

./build/test_summary.exe
Notes
The program processes the entire file sequentially
Events are parsed and passed through a simple pipeline
Detailed event printing is disabled for performance