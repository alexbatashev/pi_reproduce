#include "common.hpp"
#include "constants.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
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

  std::string outPath = std::string(kTracePathEnvVar) + opts.output().c_str();

  std::string ldLibraryPath = "LD_LIBRARY_PATH=";
  ldLibraryPath += (opts.location() / ".." / "lib").string() + ":";
  ldLibraryPath +=
      (opts.location() / ".." / "lib" / "plugin_replay").string() + ":";
  ldLibraryPath +=
      (opts.location() / ".." / "lib" / "plugin_record").string() + ":";
  ldLibraryPath +=
      (opts.location() / ".." / "lib" / "system_intercept").string() + ":";
  ldLibraryPath += std::string(std::getenv("LD_LIBRARY_PATH"));

  std::vector<const char *> cEnv;
  cEnv.reserve(opts.env().size() + 6);
  for (auto e : opts.env()) {
    if (e.find("LD_LIBRARY_PATH") == std::string::npos)
      cEnv.push_back(e.data());
  }
  cEnv.push_back("LD_PRELOAD=libsystem_intercept.so");
  cEnv.push_back("SYCL_OVERRIDE_PI_OPENCL=libplugin_replay.so");
  cEnv.push_back("SYCL_OVERRIDE_PI_LEVEL_ZERO=libplugin_replay.so");
  cEnv.push_back("SYCL_OVERRIDE_PI_CUDA=libplugin_replay.so");
  cEnv.push_back("SYCL_OVERRIDE_PI_ROCM=libplugin_replay.so");
  cEnv.push_back(ldLibraryPath.c_str());
  cEnv.push_back(outPath.c_str());
  cEnv.push_back(nullptr);

  const auto start = [&]() {
    auto err = execve(opts.input().c_str(), const_cast<char *const *>(cArgs),
                      const_cast<char *const *>(cEnv.data()));
    if (err) {
      std::cerr << "Unexpected error while running executable: " << errno
                << "\n";
    }
  };

  if (opts.no_fork()) {
    start();
  } else {
    pid_t pid = fork();
    int status;
    if (pid == 0) {
      start();
    } else {
      waitpid(pid, &status, 0);
    }
  }
}
