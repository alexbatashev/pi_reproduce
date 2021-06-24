#include "common.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

void replay(const options &opts) {
  const auto &args = opts.args();

  const char *cArgs[args.size() + 2];

  int i = 0;
  cArgs[i++] = opts.input().c_str();
  for (auto &arg : args) {
    cArgs[i++] = arg.c_str();
  }
  cArgs[i] = nullptr;

  std::string outPath =
      std::string("PI_REPRODUCE_TRACE_PATH=") + opts.output().c_str();
  std::string ldLibraryPath =
      "LD_LIBRARY_PATH=" + std::string(std::getenv("LD_LIBRARY_PATH"));
  const char *const env[] = {ldLibraryPath.c_str(),
                             "SYCL_OVERRIDE_PLUGINS=libplugin_replay.so",
                             outPath.c_str(), nullptr};
  auto err = execve(opts.input().c_str(), const_cast<char *const *>(cArgs),
                    const_cast<char *const *>(env));
  if (err) {
    std::cerr << "Unexpected error while running executable: " << errno << "\n";
  }
}
