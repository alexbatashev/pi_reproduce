// REQUIRES: linux
// RUN: %clangxx %s --std=c++17 -o %t.exe
// RUN: env TEST_ENV=1 LD_LIBRARY_PATH=TEST:$LD_LIBRARY_PATH %dpcpp_trace record --override -o %t_trace %t.exe -- %t.txt
// RUN: %dpcpp_trace pack %t_trace
// RUN: %dpcpp_trace replay %t_trace | FileCheck %s

#include <cstdlib>
#include <iostream>

int main() {
  // CHECK: LD_LIBRARY_PATH=
  std::cout << "LD_LIBRARY_PATH=" << std::getenv("LD_LIBRARY_PATH") << "\n";
  // CHECK: TEST_ENV=1
  std::cout << "TEST_ENV=" << std::getenv("TEST_ENV") << "\n";

  return 0;
}
