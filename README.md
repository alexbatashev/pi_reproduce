# pi_reproduce

pi_reproduce is a simple utility that facilitates debugging of both oneAPI DPC++
runtime and user applications that use it.

## Build instructions

## Basic usage

### Recording traces
```bash
prp record -o /path/to/output/dir ./myapp -- --app-arg1 --app-arg2=foo
```

### Printing traces
```bash
prp print /path/to/output/dir
```
