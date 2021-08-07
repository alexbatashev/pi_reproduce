// REQUIRES: opencl
//
// RUN: %clangxx -fsycl %s -o %t.exe
// RUN: env SYCL_DEVICE_FILTER="opencl" %dpcpp_trace record --override -o %t_record %t.exe
// RUN: %dpcpp_trace print %t_record | %FileCheck %s

#include <sycl/sycl.hpp>

int main() {
  sycl::queue queue;
  queue.single_task([]{});
  queue.wait();
  return 0;
}

// CHECK: Image desc:
// CHECK: Version: 2
// CHECK: Kind: 4
// CHECK: Format: None
// CHECK: Device target: spir64
// CHECK: Compile options: 
// CHECK: Link options: 
// CHECK: Offload entries [1]:
// CHECK:     Address: 0
// CHECK:     Name: _ZTSZ4mainEUlvE10000_
// CHECK:     Size: 0
// CHECK: Property sets [1]:
// CHECK:     Name: SYCL/devicelib req mask
// CHECK:     Properties [1]:
// CHECK:          Name: DeviceLibReqMask
// CHECK:          Size: 0
// CHECK:          Type: uint32
