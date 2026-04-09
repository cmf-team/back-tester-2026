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

Our primary platform is Linux, but nothing prevents it to be built and run on other OS.
The following commands are for Linux users.
Other users are encouraged to add the corresponding instructions for required steps in this README.

## Build

Install dependencies once:

```
sudo apt install -y cmake g++
```

Build using cmake:

```
cmake -B build -S .
cmake --build build -j
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

## Run

Back-tester:

```
build/bin/back-tester
```
