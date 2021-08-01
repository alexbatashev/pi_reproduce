#include "common.hpp"
#include "constants.hpp"
#include "fork.hpp"
#include "trace.hpp"
#include "utils.hpp"

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

  std::string command;
  std::string executable;
  std::vector<std::string> strArgs;
  std::vector<const char *> cArgs;

  std::string_view path{getenv("PATH")};

  if (hasCLI) {
    if (!std::filesystem::exists(opts.input())) {
      executable = which(path, opts.input().string());
    } else {
      executable = opts.input().string();
    }
    command = opts.input().string();
    const auto &args = opts.args();
    strArgs = opts.args();
    cArgs.reserve(args.size() + 2);
    cArgs.push_back(command.c_str());

    for (auto &arg : args) {
      cArgs.push_back(arg.c_str());
    }
  } else {
    if (packedReproducer) {
      executable = opts.input() / kPackedDataPath / "0";
    } else {
      executable = replayConfig[kReplayExecutable].get<std::string>();
    }
    command = replayConfig[kReplayCommand].get<std::string>();
    cArgs.push_back(command.c_str());
    for (const auto &arg : replayConfig[kReplayArguments]) {
      strArgs.push_back(arg.get<std::string>());
      cArgs.push_back(strArgs.back().c_str());
    }
  }

  cArgs.push_back(nullptr);

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
    fmt::print("{}", command);
    for (auto &arg : strArgs) {
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

  const auto start = [cArgs, cEnv, command, executable]() {
    auto err =
        execve(executable.c_str(), const_cast<char *const *>(cArgs.data()),
               const_cast<char *const *>(cEnv.data()));
    if (err) {
      std::cerr << "Unexpected error while running executable: " << errno
                << "\n";
    }
  };

  json replayFileMap;

  if (packedReproducer) {
    std::ifstream fileMap{tracePath / kReplayFileMapConfigName};
    fileMap >> replayFileMap;
    fileMap.close();
  }

  if (opts.no_fork()) {
    start();
  } else {
    pid child = fork(start);
    Tracer tracer(child);

    if (packedReproducer) {
      tracer.onFileOpen([&](const std::string &filename, const OpenHandler &h) {
        std::string newFN = replayFileMap[filename].get<std::string>();
        if (!newFN.empty()) {
          h.execute(newFN);
        } else {
          h.execute();
        }
      });
    }
  }
}
