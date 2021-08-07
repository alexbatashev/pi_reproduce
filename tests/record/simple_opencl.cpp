// REQUIRES: opencl
// RUN: %clangxx -fsycl %s -o %t.exe
// RUN: env SYCL_DEVICE_FILTER="opencl:" %dpcpp_trace record --override -o %t_record %t.exe
// RUN: ls -l %t_record | grep "0.desc"
// RUN: ls -l %t_record | grep "0_spir64.none"
// RUN: ls -l %t_record | grep "graph.json"
// RUN: ls -l %t_record | grep "env"
// RUN: ls -l %t_record | grep "replay_config.json"
// RUN: ls -l %t_record | grep "main.pi_trace"
// RUN: ls -l %t_record | grep "files_config.json"

#include <sycl/sycl.hpp>

int main() {
  sycl::queue q;
  q.single_task([]() {});
  q.wait();
  return 0;
}
