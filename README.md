# Overview

OpenFLUID simulators and observers for BVservice simulations


#Building from sources

## Requirements

The following tools and frameworks are required:
- GCC 4.9 or higher
- CMake 2.8.12 or higher
- OpenFLUID 2.1.3 or higher

## Building and testing

To build from sources, execute the following commands in the source tree using a terminal
```
mkdir _build
cd _build
cmake ..
make
```
To run the provided tests, execute the following commands from the `_build` directory using a terminal:
```
ctest
make
```
