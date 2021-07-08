#include "common.hpp"
#include "options.hpp"

#include <fmt/color.h>
#include <fmt/core.h>
#include <iostream>
#include <unistd.h>
#include <vector>

static void printInfo() {
  fmt::print("DPC++ trace utility\n");
  fmt::print("Available commands\n");
  fmt::print("{:>10} - {}\n", "info", "displays this guide");
  fmt::print("{:>10} - {}\n", "record", "records DPC++ application trace");
  fmt::print("{:>10} - {}\n", "print", "prints recorded traces");
  fmt::print("{:>10} - {}\n\n\n", "replay", "replays recorded traces");

  fmt::print(fmt::emphasis::bold, "record\n");
  fmt::print("\tUsage: dpcpp_trace record [OPTIONS] executable -- application "
             "args\n\n");
  fmt::print("\tOptions:\n");
  fmt::print("\t{:20} - {}\n", "--output, -o", "output directory, required.");
  fmt::print("\t{:20} - {}\n", "--skip-mem-objects, -s", "skip capture of memory objects.");

  fmt::print(fmt::emphasis::bold, "\n\nprint\n");
  fmt::print("\tUsage: dpcpp_trace print [OPTIONS] path/to/trace/dir\n\n");
  fmt::print("\tOptions:\n");
  fmt::print(
      "\t{:20} - {}\n", "--group <mode>",
      "group PI call traces, available modes: none, thread; default: none");
  fmt::print("\t{:20} - {}\n", "--verbose",
             "print as much info about call as possible");
  fmt::print("\t{:20} - {}\n", "--perf", "print performance summary");

  fmt::print(fmt::emphasis::bold, "\n\nreplay\n");
  fmt::print("\tUsage: dpcpp_trace replay [OPTIONS] executable -- application "
             "args\n\n");
  fmt::print("\tOptions:\n");
  fmt::print("\t{:20} - {}\n", "--output, -o",
             "path to trace directory, required");
}

int main(int argc, char *argv[], char *env[]) {
  options opts{argc, argv, env};

  if (opts.command() == options::mode::info) {
    printInfo();
  } else if (opts.command() == options::mode::record) {
    record(opts);
  } else if (opts.command() == options::mode::print) {
    printTrace(opts);
  } else if (opts.command() == options::mode::replay) {
    replay(opts);
  }
  return 0;
}
