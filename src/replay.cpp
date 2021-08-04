#include "common.hpp"
#include "constants.hpp"
#include "fork.hpp"
#include "trace.hpp"
#include "utils.hpp"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fmt/core.h>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

using json = nlohmann::json;

void replay(const options &opts) {
  std::filesystem::path tracePath;
  bool hasCLI = true;

  if (std::filesystem::is_directory(opts.input())) {
    tracePath = opts.input();
    hasCLI = false;
  } else {
    tracePath = opts.output();
  }

  std::ifstream replayConfigFile{tracePath / kReplayConfigName};
  json replayConfig;
  replayConfigFile >> replayConfig;

  bool packedReproducer = false;
  if (replayConfig[kRecordMode].get<std::string>() == kRecordModeFull) {
    packedReproducer = true;
  }

  if (hasCLI && packedReproducer) {
    throw std::runtime_error(
        "Command line arguments are not supported for packed reproducers");
  }

  std::string executable;
  std::vector<std::string> execArgs;

  const auto toString = [](std::string_view v) { return std::string{v}; };

  if (hasCLI) {
    if (!std::filesystem::exists(opts.input())) {
      executable = which(opts.input().string());
    } else {
      executable = opts.input().string();
    }
    execArgs.push_back(opts.input().string());
    std::transform(opts.args().begin(), opts.args().end(),
                   std::back_inserter(execArgs), toString);
  } else {
    if (packedReproducer) {
      executable = opts.input() / kPackedDataPath / "0";
    } else {
      executable = replayConfig[kReplayExecutable].get<std::string>();
    }
    execArgs.push_back(replayConfig[kReplayCommand].get<std::string>());
    const json jsonArgs = replayConfig[kReplayArguments];
    for (const auto &el : jsonArgs) {
      execArgs.push_back(el.get<std::string>());
    }
  }

  std::string outPath =
      std::string(kTracePathEnvVar) + "=" + tracePath.string();

  bool hasOpenCL = false;
  bool hasLevelZero = false;
  bool hasCUDA = false;
  bool hasROCm = false;

  hasOpenCL = replayConfig[kHasOpenCLPlugin].get<bool>();
  hasLevelZero = replayConfig[kHasLevelZeroPlugin].get<bool>();
  hasCUDA = replayConfig[kHasCUDAPlugin].get<bool>();
  hasROCm = replayConfig[kHasROCmPlugin].get<bool>();

  std::string ldLibraryPath = "LD_LIBRARY_PATH=";
  ldLibraryPath += (opts.location() / ".." / "lib").string() + ":";
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
    fmt::print("{}", executable);
    for (auto &arg : execArgs | std::views::drop(1)) {
      fmt::print(" {} ", arg);
    }
    fmt::print("\n");
    return;
  }

  const auto allowedEnvVar = [](std::string_view var) {
    return !var.starts_with("LD_LIBRARY_PATH");
  };

  std::vector<std::string> env;
  for (auto e : opts.env()) {
    if (allowedEnvVar(e))
      env.push_back(toString(e));
  }
  env.emplace_back("LD_PRELOAD=libsystem_intercept.so");
  if (hasOpenCL)
    env.emplace_back("SYCL_OVERRIDE_PI_OPENCL=libplugin_replay.so");
  if (hasLevelZero)
    env.emplace_back("SYCL_OVERRIDE_PI_LEVEL_ZERO=libplugin_replay.so");
  if (hasCUDA)
    env.emplace_back("SYCL_OVERRIDE_PI_CUDA=libplugin_replay.so");
  if (hasROCm)
    env.emplace_back("SYCL_OVERRIDE_PI_ROCM=libplugin_replay.so");
  env.push_back(fullLDPath);
  env.push_back(outPath);

  Tracer tracer;

  if (packedReproducer) {
    json replayFileMap;

    std::ifstream fileMap{tracePath / kReplayFileMapConfigName};
    fileMap >> replayFileMap;
    fileMap.close();

    tracer.onFileOpen([=](const std::string &filename, const OpenHandler &h) {
      if (replayFileMap.contains(filename)) {
        std::string newFN = replayFileMap[filename].get<std::string>();
        h.execute(newFN);
      } else {
        h.execute();
      }
    });
    tracer.onStat([=](const std::string &filename, const StatHandler &h) {
      if (replayFileMap.contains(filename)) {
        std::string newFN = replayFileMap[filename].get<std::string>();
        h.execute(newFN);
      } else {
        h.execute();
      }
    });
  }

  exit_code c = exec(executable, execArgs, env, tracer);
  if (c != exit_code::success)
    std::cerr << "Application exited with failure code\n";
}
