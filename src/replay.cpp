#include "common.hpp"
#include "constants.hpp"
#include "fork.hpp"
#include "utils.hpp"
#include "utils/Tracer.hpp"

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
namespace fs = std::filesystem;

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

  std::string fullLDPath = ldLibraryPath;

  std::vector<std::string> env;
  if (packedReproducer) {
    std::ifstream envIS{tracePath / "env"};
    std::string line;
    while (std::getline(envIS, line)) {
      if (line.starts_with("LD_LIBRARY_PATH=")) {
        fullLDPath += line.substr(16);
        std::cout << fullLDPath << "\n";
      } else {
        std::cout << line << "\n";
        env.push_back(line);
      }
    }
  } else {
    for (auto e : opts.env()) {
      if (allowedEnvVar(e))
        env.push_back(toString(e));
    }
    if (std::getenv("LD_LIBRARY_PATH") != nullptr) {
      fullLDPath += std::string(std::getenv("LD_LIBRARY_PATH"));
    }
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

  dpcpp_trace::NativeTracer tracer;

  if (packedReproducer) {
    json replayFileMap;

    std::ifstream fileMap{tracePath / kReplayFileMapConfigName};
    fileMap >> replayFileMap;
    fileMap.close();

    const auto findSuitableReplacement =
        [=](std::string_view name) -> std::string {
      const fs::path basePath = tracePath / kPackedDataPath;
      if (name.find(".so") != std::string::npos) {
        // Linux does not check for library locations, that do not exist.
        // We need to be a bit more creative here.
        std::string libName = fs::path(name).filename().string();
        for (const auto &el : replayFileMap.items()) {
          std::string cand = el.key();
          if (cand.find(libName) != std::string::npos) {
            return basePath / el.value();
          }
        }
      } else {
        if (replayFileMap.contains(name)) {
          return basePath / replayFileMap[std::string{name}];
        }
      }
      return "";
    };

    tracer.onFileOpen(
        [=](std::string_view filename, const dpcpp_trace::OpenHandler &h) {
          std::string replacement = findSuitableReplacement(filename);
          if (!replacement.empty())
            h.replaceFilename(replacement);
        });
    tracer.onStat(
        [=](std::string_view filename, const dpcpp_trace::StatHandler &h) {
          std::string replacement = findSuitableReplacement(filename);
          if (!replacement.empty())
            h.replaceFilename(replacement);
        });
  }

  tracer.launch(executable, execArgs, env);
  tracer.start();
  tracer.wait();
}
