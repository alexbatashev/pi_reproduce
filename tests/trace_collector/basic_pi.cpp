// REQUIRES: opencl
// RUN: %clangxx -fsycl %s -lpthread -o %t.exe
// RUN: mkdir -p %t_trace
// RUN: env LD_PRELOAD=libsystem_intercept.so env SYCL_DEVICE_FILTER="opencl:" env XPTI_TRACE_ENABLE=1 env XPTI_FRAMEWORK_DISPATCHER=libxptifw.so env XPTI_SUBSCRIBERS=libtrace_collector.so env DPCPP_TRACE_DATA_PATH=%t_trace %t.exe
// RUN: %trace_printer %t_trace/main.pi_trace | FileCheck %s --check-prefix=MAIN
// RUN: %trace_printer %t_trace/main_2.pi_trace | FileCheck %s --check-prefix=MAIN2

#include <sycl/sycl.hpp>
#include <thread>

int main() {
  sycl::queue q{sycl::default_selector{}};

  const auto test = [&q] {
      void *p = sycl::malloc_device<char>(100, q);
      sycl::free(p, q);
  };

  std::thread t = std::thread(test);

  test();

  if (t.joinable())
    t.join();

  return 0;
}

// MAIN: piPlatformsGet
// MAIN: piPlatformsGet
// MAIN: piDevicesGet
// MAIN: piDevicesGet
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceRetain
// MAIN: piPlatformGetInfo
// MAIN: piPlatformGetInfo
// MAIN: piDeviceRelease
// MAIN: piDevicesGet
// MAIN: piDevicesGet
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceRetain
// MAIN: piPlatformGetInfo
// MAIN: piPlatformGetInfo
// MAIN: piDeviceRelease
// MAIN: piDevicesGet
// MAIN: piDevicesGet
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceRetain
// MAIN: piPlatformGetInfo
// MAIN: piPlatformGetInfo
// MAIN: piDeviceRelease
// MAIN: piDevicesGet
// MAIN: piDevicesGet
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceRetain
// MAIN: piDevicesGet
// MAIN: piDevicesGet
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceRetain
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceGetInfo
// MAIN: piDeviceRelease
// MAIN: piContextCreate
// MAIN: piQueueCreate
// MAIN: piextUSMDeviceAlloc
// MAIN: piextUSMFree
// MAIN: piQueueRelease
// MAIN: piContextRelease
// MAIN: piDeviceRelease
// MAIN: piTearDown

// MAIN2: piextUSMDeviceAlloc
// MAIN2: piextUSMFree
