// REQUIRES: linux
// RUN: echo "1" > %t.txt
// RUN: %clangxx %s --std=c++17 -o %t.exe
// RUN: %dpcpp_trace record --override -o %t_trace %t.exe -- %t.txt
// RUN: rm %t.txt
// RUN: %dpcpp_trace pack %t_trace
// RUN: %dpcpp_trace replay %t_trace

#include <cstdlib>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

int main(int argc, char *argv[]) {
  if (argc != 2)
    return EXIT_FAILURE;

  if (!fs::exists(argv[1]))
    return EXIT_FAILURE;

  std::ifstream is{argv[1]};
  int test;
  is >> test;
  if (test != 1) {
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}
