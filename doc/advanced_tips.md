# Advanced usage tips

## Using with rr

rr is a reverse debugger. See https://rr-project.org/ for more info.

Unfortunately, rr does not support recording system calls, that are required for
GPU drivers to work. Neither it supports `execve` call, that is required for PI
replay with dpcpp\_trace.

The `execve` issue can be avoided if you run the replay steps directly, without
`dpcpp_trace replay` wrapper command. To figure out, what is actually being
executed, the tool supports `-p` flag. A sample usage would be:

```bash
$ dpcpp_trace replay -o my_record -p app -- --work-size=1000

DPCPP_TRACE_DATA_PATH=my_record \
LD_LIBRARY_PATH=/some/path/to/libs/:$LD_LIBRARY_PATH \
LD_PRELOAD=libsystem_intercept.so \
SYCL_OVERRIDE_PI_OPENCL=libplugin_replay.so \
SYCL_OVERRIDE_PI_LEVEL_ZERO=libplugin_replay.so \
SYCL_OVERRIDE_PI_CUDA=libplugin_replay.so \
SYCL_OVERRIDE_PI_ROCM=libplugin_replay.so \
app --work-size=1000
```

Simply add `rr record` before `app` to record your trace.

**NOTE** There may be other issues, related to your host hardware or
application. Refer to rr documentation to resolve them.

## Converting execution graph to dot format

Use `graph2dot.py` script to perform conversion:

```bash
python tools/graph2dot.py path/to/trace/graph.json out.dot
```
