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

## Build

Install dependencies once:

```
sudo apt install -y cmake g++ libtool autoconf lcov
```

Build using cmake:

```
cmake -B build -S .
cmake --build build -v -j
```

or

```
mkdir -p build
pushd build
cmake ..
make -j VERBOSE=1
popd
```

## Test

To run unit tests:

```
ctest --test-dir build -j
```

or

```
pushd build
ctest -j
popd
```

or

```
build/bin/test/back-tester-tests
```

## Coverage

- Build with `-DENABLE_COVERAGE=ON`
- Run the tests
- Run

```
cmake --build build -t coverage
```

or

```
pushd build
lcov --capture --directory ./src  --exclude '/usr/include/*' --exclude '/usr/lib/*' \
     --exclude '3rdparty/*' --exclude 'build/include/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage_report | grep % | head -1 | cut -f 4 -d " "
popd
```

This will generate html report in build/coverage_report/index.html and will print a single number for
line number coverage percentage to the stdout.

## Run

Back-tester:

```
build/bin/back-tester
```
