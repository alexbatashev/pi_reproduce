#include "common.hpp"
#include "constants.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using json = nlohmann::json;

void replay(const options &opts) {
  const auto &args = opts.args();

  const char *cArgs[args.size() + 2];

  int i = 0;
  cArgs[i++] = opts.input().c_str();
  for (auto &arg : args) {
    cArgs[i++] = arg.c_str();
  }
  cArgs[i] = nullptr;

  std::string outPath = std::string(kTracePathEnvVar) + "=" + opts.output().c_str();

  bool hasOpenCL = false;
  bool hasLevelZero = false;
  bool hasCUDA = false;
  bool hasROCm = false;

  std::ifstream replayConfigFile{opts.output() / kReplayConfigName};
  json replayConfig;
  replayConfigFile >> replayConfig;
  hasOpenCL = replayConfig[kHasOpenCLPlugin].get<bool>();
  hasLevelZero = replayConfig[kHasLevelZeroPlugin].get<bool>();
  hasCUDA = replayConfig[kHasCUDAPlugin].get<bool>();
  hasROCm = replayConfig[kHasROCmPlugin].get<bool>();

  std::string ldLibraryPath = "LD_LIBRARY_PATH=";
  ldLibraryPath += (opts.location() / ".." / "lib").string() + ":";
  ldLibraryPath +=
      (opts.location() / ".." / "lib" / "plugin_replay").string() + ":";
  ldLibraryPath +=
      (opts.location() / ".." / "lib" / "plugin_record").string() + ":";
  ldLibraryPath +=
      (opts.location() / ".." / "lib" / "system_intercept").string() + ":";
  std::string fullLDPath = ldLibraryPath;
  if (std::getenv("LD_LIBRARY_PATH")) {
    fullLDPath += std::string(std::getenv("LD_LIBRARY_PATH"));
  }

  if (opts.print_only()) {
    fmt::print("{} \\\n", outPath);
    fmt::print("{}$LD_LIBRARY_PATH \\\n", ldLibraryPath);
    fmt::print("LD_PRELOAD=libsystem_intercept.so \\\n");
    if (hasOpenCL)
      fmt::print("SYCL_OVERRIDE_PI_OPENCL=libplugin_replay.so \\\n");
    if (hasLevelZero)
      fmt::print("SYCL_OVERRIDE_PI_LEVEL_ZERO=libplugin_replay.so \\\n");
    if (hasCUDA)
      fmt::print("SYCL_OVERRIDE_PI_CUDA=libplugin_replay.so \\\n");
    if (hasROCm)
      fmt::print("SYCL_OVERRIDE_PI_ROCM=libplugin_replay.so \\\n");
    fmt::print("{}", opts.input().string());
    for (auto &arg : opts.args()) {
      fmt::print(" {} ", arg);
    }
    fmt::print("\n");
    return;
  }

  std::vector<const char *> cEnv;
  cEnv.reserve(opts.env().size() + 6);
  for (auto e : opts.env()) {
    if (e.find("LD_LIBRARY_PATH") == std::string::npos)
      cEnv.push_back(e.data());
  }
  cEnv.push_back("LD_PRELOAD=libsystem_intercept.so");
  if (hasOpenCL)
    cEnv.push_back("SYCL_OVERRIDE_PI_OPENCL=libplugin_replay.so");
  if (hasLevelZero)
    cEnv.push_back("SYCL_OVERRIDE_PI_LEVEL_ZERO=libplugin_replay.so");
  if (hasCUDA)
    cEnv.push_back("SYCL_OVERRIDE_PI_CUDA=libplugin_replay.so");
  if (hasROCm)
    cEnv.push_back("SYCL_OVERRIDE_PI_ROCM=libplugin_replay.so");
  cEnv.push_back(fullLDPath.c_str());
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
