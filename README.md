# DPC++ trace utility

[![CI pipeline](https://github.com/alexbatashev/dpcpp_trace/actions/workflows/ci.yaml/badge.svg?event=schedule)](https://github.com/alexbatashev/dpcpp_trace/actions/workflows/ci.yaml?query=event%3Aschedule)

**!!!WARNING!!! UNDER CONSTRUCTION**

dpcpp_trace is a simple utility that facilitates debugging of both oneAPI DPC++
runtime and user applications that use it.

**Features**

- Record PI call traces
- Collect performance statistics
- Replay responses from plugins

## Getting pre-built binaries

Pre-built binaries are published to for every nightly release [here](https://github.com/alexbatashev/dpcpp_trace/actions/workflows/ci.yaml?query=event%3Aschedule).

## Build instructions

1. Download and build DPC++: https://intel.github.io/llvm-docs/GetStartedGuide.html#build-dpc-toolchain

2. Download and build dpcpp_trace:

```bash
git clone --recursive https://github.com/alexbatashev/dpcpp_trace.git
cd dpcpp_trace
mkdir build && cd build
cmake -DINTEL_LLVM_SOURCE_PATH=/path/to/intel/llvm/src/dir -DINTEL_LLVM_BINARY_PATH=/path/to/intel/llvm/build/dir -GNinja ..
ninja
```

## Basic usage

### Recording traces
```bash
dpcpp_trace record -o /path/to/output/dir ./myapp -- --app-arg1 --app-arg2=foo
```

### Printing traces
Simple mode (similar to `SYCL_PI_TRACE=-1`):

```bash
dpcpp_trace print /path/to/output/dir
```

Group per thread:

```bash
dpcpp_trace print /path/to/output/dir --group thread
```

Verbose mode:

```bash
dpcpp_trace print /path/to/output/dir --verbose
```

Performance summary:

```bash
dpcpp_trace print /path/to/output/dir --perf
```

### Replaying traces
```bash
dpcpp_trace replay -o /path/to/output/dir ./myapp -- --app-arg1 --app-arg2=foo
```
