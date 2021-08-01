// REQUIRES: opencl
// RUN: %clangxx -fsycl %s -o %t.exe
// RUN: env SYCL_DEVICE_FILTER="opencl" %dpcpp_trace record --override -o %t_record %t.exe
// RUN: env SYCL_DEVICE_FILTER="opencl" %dpcpp_trace replay %t_record | FileCheck %s

#include <sycl/sycl.hpp>

int main() {
  constexpr size_t Size = 10;
  sycl::queue queue;
  // CHECK: Created queue
  std::cout << "Created queue" << std::endl;
  // CHECK: Device name
  std::cout << "Device name: ";
  std::cout << queue.get_device().get_info<sycl::info::device::name>()
            << std::endl;

  std::vector<int> a, b;
  a.resize(Size);
  b.resize(Size);

  sycl::buffer aBuf{a.begin(), a.end()};
  sycl::buffer bBuf{b.begin(), b.end()};

  queue.submit([&](sycl::handler &cgh) {
    sycl::accessor in{aBuf, cgh, sycl::read_only};
    sycl::accessor out{bBuf, cgh, sycl::write_only, sycl::no_init};
    cgh.parallel_for(sycl::range(Size),
                     [=](sycl::id<1> id) { out[id] = in[id]; });
  });

  // CHECK: Task submitted
  std::cout << "Task submitted" << std::endl;

  queue.wait();

  // CHECK: Success
  std::cout << "Success" << std::endl;
  return 0;
}
