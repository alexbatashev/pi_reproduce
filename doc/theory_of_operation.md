# Theory of operation

## Capturing PI call trace

`dpcpp_trace` uses XPTI to intercept SYCL runtime calls to Plugin Interface and
captures API call arguments. The `record` command inserts a few environment
variables with the help of `execve` call:

- `XPTI_TRACE_ENABLE=1` to enable tracing inside DPC++ runtime.
- `XPTI_FRAMEWORK_DISPATCHER=libxptifw.so` to specify the event dispatcher.
- `XPTI_SUBSCRIBERS=libplugin_record.so` to specify the subscriber library,
  which does most of the work.

The subscriber library listens to `sycl.pi.debug` stream, and records all
arguments into a binary file, which format is described below.

To learn more about XPTI, please, refer to
[xpti](https://github.com/intel/llvm/blob/sycl/xpti/doc/SYCL_Tracing_Implementation.md)
and [xptifw](https://github.com/intel/llvm/blob/sycl/xptifw/doc/XPTI_Framework.md)
documentation.

### Binary trace file format

Each `.trace` file is composed of PI call records. Each record has the
following format:
```
uint32_t function_id -- as defined by PiApiKind enum
uint8_t backend -- backend id, same as sycl::backend.
uint64_t start -- call start timestamp in microseconds.
uint64_t end -- call end timestamp in microseconds.
size_t num_inputs -- number of recorded input arguments.
size_t num_outputs -- number of recorded output arguments.
size_t total_args_size -- number of bytes of recorded input bytes
--------
<total_args_size bytes of raw data> -- arguments values
--------
-------- - repeated num_inputs times
size_t input_size -- number of bytes of recorded data
<input_size bytes of raw data> -- input data
--------
-------- - repeated num_outputs times
size_t output_size -- number of bytes of recorded data
<output_size bytes of raw data> -- output data
--------
pi_result return_value -- the return value of API call
```

### Handling string arguments

Some PI APIs accept C-style strings as input parameters. In that case the whole
string with terminating `\0` is stored in the arguments data, and the size of
the data block will be equal to `sizeof` of other arguments + length of the
string.

### Handling multithreading

`dpcpp_trace` records traces per each thread. To be able to reliably
distinguish between thread, the tool injects a library, that intercepts
`pthread_create` function and assigns each thread a unique name. The first
thread is always named `main`. On thread creation, newly thread is assigned name
in format `<current_thread_name>_n`, where `n` is the index number of the
thread, started by current thread. For each thread a file with thread name is
created. When printing, `dpcpp_trace` tool can either show traces per thread,
or sort records by API call time.

### Device images

Device images are dumped to the output directory on the first call to
`piextDeviceSelectBinary`. The following naming scheme is used:
```
<index of binary image>_<target_arch>.<extension>
```

Where extension is one of the following:
- `PI_DEVICE_BINARY_TYPE_SPIRV` -> `.spv`
- `PI_DEVICE_BINARY_TYPE_NATIVE` -> `.bin`
- `PI_DEVICE_BINARY_TYPE_LLVMIR_BITCODE` -> `.bc`
- `PI_DEVICE_BINARY_TYPE_NONE` -> `.none`

TBD describe device image descriptors.

## Replaying PI traces

### Emulating plugins

Plugin emulation relies on SYCL runtime's plugin override capability.
`dpcpp_trace` provides a PI plugin implementation, that reads trace files and
responses PI calls with that info.

Trace files are read sequentially, so, it is essential for the program to have
the same environment and command line arguments.

### Emulating DPC++ runtime
TBD

## Packed reproducers

### Recording (Linux)
On Linux `dpcpp_trace` uses `ptrace` function to intercept `openat` system call.
Each opened file is recorded into `files_config.json` file.

### Packing
When trace is recorded, a separate `dpcpp_trace pack` run can be performed to
copy application executable and its dependencies into trace directory. A special
`replay_file_map.json` file is composed to provide mapping between original
files and their packed versions. On Linux, paths, that start with `/dev`,
`/sys`, or `/proc` are skipped.

