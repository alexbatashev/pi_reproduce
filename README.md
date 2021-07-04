# pi_reproduce

**!!!WARNING!!! UNDER CONSTRUCTION**

pi_reproduce is a simple utility that facilitates debugging of both oneAPI DPC++
runtime and user applications that use it.

**Features**

- Record PI call traces
- Collect performance statistics
- Replay responses from plugins

## Build instructions

**NOTE: Not all patches, required for pi_reproduce to work have been upstreamed**

1. Download and build DPC++: https://intel.github.io/llvm-docs/GetStartedGuide.html#build-dpc-toolchain

2. Download and build pi_reproduce:

```bash
git clone --recursive https://github.com/alexbatashev/pi_reproduce.git
cd pi_reproduce
mkdir build && cd build
cmake -DINTEL_LLVM_PATH=/path/to/intel/llvm/src/dir -GNinja ..
ninja
```

## Basic usage

### Recording traces
```bash
prp record -o /path/to/output/dir ./myapp -- --app-arg1 --app-arg2=foo
```

### Printing traces
Simple mode (similar to `SYCL_PI_TRACE`):

```bash
prp print /path/to/output/dir
```

Group per thread:

```bash
prp print /path/to/output/dir --group thread
```

Verbose mode:

```bash
prp print /path/to/output/dir --verbose
```

Performance summary:

```bash
prp print /path/to/output/dir --perf
```
