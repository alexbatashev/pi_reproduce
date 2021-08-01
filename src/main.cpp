#include "common.hpp"
#include "options.hpp"

#include <fmt/color.h>
#include <fmt/core.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
#include <vector>

inline constexpr auto infoText = R"___(
DPC++ trace utility

Available commands:

    info    displays this guide
    record  records DPC++ application trace
    print   prints recorded traces
    replay  replays recorded PI traces
    pack    packs executable and its dependencies into trace

- record:
    Usage: dpcpp_trace record [OPTIONS] executable [-- application args]

    Options:
      --output, -o  output directory, required.
      --skip-mem-objects, -s
                    skip record of memory objects.

- print:
    Usage: dpcpp_trace print [OPTIONS] path/to/trace/dir

    Options:
      --group <mode>
                   group PI call traces, available modes: none, thread;
                   default: none.
      --verbose    print as much info as possible.
      --perf       print performance summary per group.

- replay:
    Usages: dpcpp_trace replay [OPTIONS] path/to/trace/dir
            dpcpp_trace replay [OPTIONS] -t /path/to/trace executable [-- args]

    Options:
      -t <path>    use trace from this path, but command line is specified
                   separately.
      --print-only, -p
                   print command, that is going to be executed.

- pack:
    Usage: dpcpp_trace pack /path/to/trace
)___";

static void printInfo() { fmt::print(infoText); }

int main(int argc, char *argv[], char *env[]) {
  try {
    options opts{argc, argv, env};

    if (opts.command() == options::mode::info) {
      printInfo();
    } else if (opts.command() == options::mode::record) {
      record(opts);
    } else if (opts.command() == options::mode::print) {
      printTrace(opts);
    } else if (opts.command() == options::mode::replay) {
      replay(opts);
    } else if (opts.command() == options::mode::pack) {
      pack(opts);
    } else if (opts.command() == options::mode::debug) {
      debug(opts);
    }
  } catch (std::runtime_error &err) {
    std::cerr << "CLI error: " << err.what() << "\n";
    return -1;
  }
  return 0;
}
