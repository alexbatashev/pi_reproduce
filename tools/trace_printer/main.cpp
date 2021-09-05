#include "utils/protobuf_stream.hpp"
#include "api_call.pb.h"

#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;
using namespace dpcpp_trace;

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "trace_printer requires 1 argument\n";
    return 1;
  }

  fs::path path{argv[1]};

  if (!fs::exists(path)) {
    std::cerr << "File does not exist " << path << "\n";
    return 1;
  }

  for (const auto &call : protobuf_stream<APICall>(path)) {
    // TODO use unified printers.
    std::cout << call.call_name() << "\n";
  }

  std::cout << std::endl;

  return 0;
}
