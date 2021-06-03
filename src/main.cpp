#include "options.hpp"
#include "common.hpp"

#include <iostream>
#include <vector>
#include <unistd.h>

static void printInfo() {
  std::cout << "pi reproducer\n";
  std::cout << "\nCommands:\n\n";
  std::cout << "\trecord   records execution information\n";
  std::cout << "\tprint    prints trace\n";
  std::cout << "\treplay   executes trace\n";
  std::cout << "\tinfo     prints this message\n";
}

int main(int argc, char *argv[]) {
  options opts{argc, argv};

  if (opts.command() == options::mode::info) {
    printInfo();
  } else if (opts.command() == options::mode::record) {
    record(opts);
  } else if (opts.command() == options::mode::print) {
    printTrace(opts);
  }
  return 0;
}
