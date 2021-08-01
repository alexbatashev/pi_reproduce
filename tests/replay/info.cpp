// RUN: %clangxx -fsycl %s -o %t.exe
// RUN: %dpcpp_trace record --override -o %t_trace %t.exe
// RUN: %dpcpp_trace replay %t_trace | %FileCheck %s

#include <sycl/sycl.hpp>

int main() {
  using namespace sycl;
  platform p;
  // CHECK: Platform name
  std::cout << "Platform name: " << p.get_info<info::platform::name>() << std::endl;
  // CHECK: Platform version
  std::cout << "Platform version: " << p.get_info<info::platform::version>() << std::endl;
  // CHECK: Platform vendor
  std::cout << "Platform vendor: " << p.get_info<info::platform::vendor>() << std::endl;

  device d;
  // CHECK: Device name
  std::cout << "Device name: " << d.get_info<info::device::name>() << std::endl;
  // CHECK: Device version
  std::cout << "Device version: " << d.get_info<info::device::version>() << std::endl;
  // CHECK: Device vendor
  std::cout << "Device vendor: " << d.get_info<info::device::vendor>() << std::endl;

  // CHECK: Success
  std::cout << "Success\n";

  return 0;
}
