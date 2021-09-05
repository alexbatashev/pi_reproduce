// REQUIRES: opencl, TEMPORARY_DISABLED
// RUN: %clangxx -fsycl %s -lpthread -lOpenCL -o %t.exe
// RUN: mkdir -p %t_trace
// RUN: env LD_PRELOAD=libsystem_intercept.so env OPENCL_LAYERS=libocl_xpti_layer.so env XPTI_TRACE_ENABLE=1 env XPTI_FRAMEWORK_DISPATCHER=libxptifw.so env XPTI_SUBSCRIBERS=libtrace_collector.so env DPCPP_TRACE_DATA_PATH=%t_trace %t.exe
// RUN: %trace_printer %t_trace/main.ocl_trace | FileCheck %s --check-prefix=MAIN

#include <CL/cl.h>
#include <vector>

int main() {
  std::vector<cl_platform_id> platforms;
  cl_uint size = 0;
  clGetPlatformIDs(0, nullptr, &size);
  platforms.resize(size);
  clGetPlatformIDs(size, platforms.data(), nullptr);

  return 0;
}

// MAIN: clGetPlatformIDs
// MAIN: clGetPlatformIDs
