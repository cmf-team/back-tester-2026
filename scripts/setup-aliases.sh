#!/bin/bash
# Setup aliases and functions for common development tasks
# Source this file: source scripts/setup-aliases.sh

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# ═══════════════════════════════════════════════════════════════════════════════
# Build Commands
# ═══════════════════════════════════════════════════════════════════════════════

# cbb: cmake build abbreviated - build the project with flexible parameters
alias cbb='cmake --build build'

# Function to build the project (wrapper around cbb)
function build() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Building project...${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    cmake --build build -j 4 "$@"
    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ Build successful${NC}"
    else
        echo -e "${RED}✗ Build failed${NC}"
    fi
    return $exit_code
}

# ═══════════════════════════════════════════════════════════════════════════════
# Test Commands
# ═══════════════════════════════════════════════════════════════════════════════

# Function to build and run tests with verbose failure reporting
function test() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Building tests...${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    cmake --build build -j 4 --target back-tester-tests
    local build_exit=$?

    if [ $build_exit -ne 0 ]; then
        echo -e "${RED}✗ Test build failed${NC}"
        return 1
    fi

    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Running tests...${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    # Run tests and capture output
    local test_output=$(build/bin/test/back-tester-tests 2>&1)
    local test_exit=$?

    # Print the full output
    echo "$test_output"

    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    # Parse and report summary
    if echo "$test_output" | grep -q "All tests passed"; then
        echo -e "${GREEN}✓ All tests passed${NC}"
        # Extract assertion and test counts
        local summary=$(echo "$test_output" | grep "All tests passed")
        echo -e "${GREEN}  $summary${NC}"
        echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        return 0
    else
        # Check for failed tests
        if echo "$test_output" | grep -q "FAILED"; then
            echo -e "${RED}✗ Some tests failed:${NC}"
            echo "$test_output" | grep -A 2 "FAILED:" | head -20
        fi

        # Check for skipped tests
        if echo "$test_output" | grep -q "skipped"; then
            echo -e "${YELLOW}⊘ Skipped tests:${NC}"
            echo "$test_output" | grep "skipped"
        fi

        # Show test summary
        local summary=$(echo "$test_output" | grep -E "test cases:|assertions:")
        if [ -n "$summary" ]; then
            echo -e "${YELLOW}Summary:${NC}"
            echo "$summary"
        fi

        echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
        return 1
    fi
}

# Alias for quick test execution
alias test-quick='build/bin/test/back-tester-tests'
alias test-verbose='build/bin/test/back-tester-tests -s'

# ═══════════════════════════════════════════════════════════════════════════════
# Benchmark Commands
# ═══════════════════════════════════════════════════════════════════════════════

# Function to build and run benchmarks
function bench() {
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Configuring with benchmarks enabled...${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    cmake -B build -S . -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release > /dev/null
    local config_exit=$?

    if [ $config_exit -ne 0 ]; then
        echo -e "${RED}✗ CMake configuration failed${NC}"
        return 1
    fi

    echo -e "${BLUE}Building benchmarks...${NC}"
    cmake --build build -j 4 --target back-tester-bench
    local build_exit=$?

    if [ $build_exit -ne 0 ]; then
        echo -e "${RED}✗ Benchmark build failed${NC}"
        return 1
    fi

    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${BLUE}Running benchmarks...${NC}"
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    build/bin/bench/back-tester-bench "$@"
    local bench_exit=$?

    echo ""
    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"

    if [ $bench_exit -eq 0 ]; then
        echo -e "${GREEN}✓ Benchmarks completed${NC}"
    else
        echo -e "${RED}✗ Benchmarks failed${NC}"
    fi

    echo -e "${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    return $bench_exit
}

# Alias for running benchmarks directly (if already built)
alias bench-run='build/bin/bench/back-tester-bench'

# ═══════════════════════════════════════════════════════════════════════════════
# Combined Commands
# ═══════════════════════════════════════════════════════════════════════════════

# Full build: configure, build, and test
function full-build() {
    echo -e "${BLUE}╔═══════════════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║                     FULL BUILD AND TEST PIPELINE                             ║${NC}"
    echo -e "${BLUE}╚═══════════════════════════════════════════════════════════════════════════════╝${NC}"

    # Configure
    echo ""
    echo -e "${BLUE}[1/3] Configuring CMake...${NC}"
    cmake -B build -S . || return 1

    # Build
    echo ""
    echo -e "${BLUE}[2/3] Building project...${NC}"
    build || return 1

    # Test
    echo ""
    echo -e "${BLUE}[3/3] Running tests...${NC}"
    test || return 1

    echo ""
    echo -e "${GREEN}╔═══════════════════════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${GREEN}║                    ✓ FULL BUILD SUCCESSFUL                                  ║${NC}"
    echo -e "${GREEN}╚═══════════════════════════════════════════════════════════════════════════════╝${NC}"
}

# ═══════════════════════════════════════════════════════════════════════════════
# Help
# ═══════════════════════════════════════════════════════════════════════════════

function dev-help() {
    cat << 'EOF'
╔═══════════════════════════════════════════════════════════════════════════════╗
║                    DEVELOPMENT COMMAND REFERENCE                             ║
╚═══════════════════════════════════════════════════════════════════════════════╝

BUILD COMMANDS:
  cbb [args]              Abbreviation for: cmake --build build [args]
                          Usage: cbb -j 4

  build [args]            Build the project with standard settings
                          Usage: build
                          Usage: build -j 8

TESTING:
  test                    Build and run all tests with verbose reporting
                          Shows passed/failed/skipped tests with colors
                          Usage: test

  test-quick              Run tests directly (must be built first)
                          Usage: test-quick

  test-verbose            Run tests with verbose output
                          Usage: test-verbose

BENCHMARKING:
  bench [args]            Build (with benchmarks enabled) and run benchmarks
                          Usage: bench
                          Usage: bench --benchmark_filter=FlatMerger

  bench-run [args]        Run benchmarks directly (must be built first)
                          Usage: bench-run

COMBINED:
  full-build              Full pipeline: configure → build → test
                          Usage: full-build

HELP:
  dev-help                Show this help message

═════════════════════════════════════════════════════════════════════════════════

EXAMPLES:
  # Build with 8 threads
  cbb -j 8

  # Run tests with verbose reporting
  test

  # Run only DataIngestion tests
  test-quick "[DataIngestion]"

  # Run benchmarks with filter
  bench --benchmark_filter=Hierarchy

  # Full development cycle
  full-build

═════════════════════════════════════════════════════════════════════════════════
EOF
}

# Print setup message
echo -e "${GREEN}✓ Development aliases loaded successfully${NC}"
echo -e "${YELLOW}  Type 'dev-help' for command reference${NC}"
