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
// CHECK: Offload entries [2]:
// CHECK:     Address: 0
// CHECK:     Name: _ZTSN2cl4sycl6detail16AssertInfoCopierE
// CHECK:     Size: 0
// CHECK:     Address: 0
// CHECK:     Name: _ZTSZ4mainEUlvE_
// CHECK:     Size: 0
// CHECK: Property sets [1]:
// CHECK:     Name: SYCL/devicelib req mask
// CHECK:     Properties [1]:
// CHECK:          Name: DeviceLibReqMask
// CHECK:          Size: 0
// CHECK:          Type: uint32
