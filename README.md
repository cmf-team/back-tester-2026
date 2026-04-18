# CMF Advanced Backtesting Engine for Options

## Directory structure

```
.
├── 3rdparty                    # place holder for 3rd party libraries (downloaded during the build)
├── build                       # local build tree used by CMake
├    ├── bin                    # generated binaries
├    ├── lib                    # generated libs (including those, which are built from 3rd party sources)
├    ├── cfg                    # generated config files (if any)
├    └── include                # generated include files (installed during the build for 3rd party sources)
├── cmake                       # cmake helper scripts
├── config                      # example config files
├── scripts                     # shell (and other) maintenance scripts
├── src                         # source files
├    ├── common                 # common utility files
├    ├── ...                    # ...
├    └── main                   # main() for back-tester app
├── test                        # unit-tests and other tests
├── CMakeLists.txt              # main build script
└── README.md                   # this README
```

## OS

Our primary platform is Linux, but the project also builds and runs on macOS.

### Linux

Install dependencies once:

```bash
sudo apt install -y cmake g++
```

### macOS

Install dependencies once via [Homebrew](https://brew.sh):

```bash
brew install cmake
```

Xcode Command Line Tools provide the compiler (`clang++`). Install them if you haven't already:

```bash
xcode-select --install
```

## Build

```bash
cmake -B build -S . -DBUILD_TESTS=ON
cmake --build build -j
```

## Test

```bash
ctest --test-dir build --output-on-failure -j
```

Or run the test binary directly (supports Catch2 tag filters):

```bash
build/bin/test/back-tester-tests                  # all tests
build/bin/test/back-tester-tests "[BasicTypes]"   # single tag
```

## Run

```bash
build/bin/back-tester <path-to-daily-ndjson-file>
```

Example:

```bash
build/bin/back-tester data/XEUR-20260409-HTT6HHLT6R/xeur-eobi-20260309.mbo.json
```

## Contributing

Install UV, create a virtual environment, and install the project dependencies:

```bash
curl -LsSf https://astral.sh/uv/install.sh | sh
uv sync
```

Then activate the virtual environment and set up the git pre-commit hooks:

```bash
source .venv/bin/activate
pre-commit install
```

After that, formatting and linting will run automatically before each commit.  
If the source code does not meet the required formatting rules, the hook will 
modify the files and stop the commit, and you will need to stage the updated 
changes manually.

To run formatting and linting yourself, use one of these commands:

```bash
pre-commit run --files file.py
pre-commit run --all-files
```

The current pre-commit hooks do the following:
- format and lint C++ code with `clang-format`;
- format and lint Python code with `ruff`;
- strip outputs from Jupyter notebooks.
