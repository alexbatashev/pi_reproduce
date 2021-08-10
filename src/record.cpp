#include "common.hpp"
#include "constants.hpp"
#include "fork.hpp"
#include "utils/Tracer.hpp"
#include "utils.hpp"

#include <cstdlib>
#include <dlfcn.h>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <nlohmann/json.hpp>
#include <ranges>
#include <string>
#include <string_view>

using json = nlohmann::json;

static bool canLoadLibrary(std::string_view libName) {
  void *handle = dlopen(libName.data(), RTLD_LAZY);
  bool res = false;
  if (handle) {
    res = true;
    dlclose(handle);
  }
  return res;
}

void record(const options &opts) {
  if (std::filesystem::exists(opts.output())) {
    if (opts.record_override_trace()) {
      std::clog << "WARNING: output path exists and will be removed: "
                << opts.output() << "\n";
      std::filesystem::remove_all(opts.output());
    } else {
      std::cerr << "Output path " << opts.output() << " already exists\n";
      std::terminate();
    }
  }
  if (!std::filesystem::create_directory(opts.output())) {
    std::cerr << "Failed to create directory " << opts.output() << "\n";
    std::terminate();
  }

  if (!std::filesystem::create_directory(opts.output() / kBuffersPath)) {
    std::cerr << "Failed to create directory " << opts.output() / kBuffersPath
              << "\n";
    exit(EXIT_FAILURE);
  }

  std::ofstream envFile{opts.output() / "env"};
  for (auto entry : opts.env()) {
    envFile << entry << "\n";
  }
  envFile.close();

  std::string executable = opts.input().string();

  if (!std::filesystem::exists(opts.input())) {
    executable = which(executable);
  }

  const auto &args = opts.args();

  {
    std::ofstream replayConfigFile{opts.output() / kReplayConfigName};
    json replayConfig;
    replayConfig[kHasOpenCLPlugin] = canLoadLibrary(kOpenCLPluginName);
    replayConfig[kHasLevelZeroPlugin] = canLoadLibrary(kLevelZeroPluginName);
    replayConfig[kHasCUDAPlugin] = canLoadLibrary(kCUDAPluginName);
    replayConfig[kHasROCmPlugin] = canLoadLibrary(kROCmPluginName);
    if (opts.record_skip_mem_objects())
      replayConfig[kRecordMode] = kRecordModeTraceOnly;
    else
      replayConfig[kRecordMode] = kRecordModeDefault;

    replayConfig[kReplayCommand] = opts.input().string();
    replayConfig[kReplayExecutable] = executable;

    replayConfig[kReplayArguments] = json::array();
    for (const auto &arg : args) {
      replayConfig[kReplayArguments].push_back(arg);
    }

    replayConfigFile << replayConfig.dump(4);
    replayConfigFile.close();
  }

  const auto toString = [](std::string_view v) { return std::string{v}; };

  std::vector<std::string> execArgs;
  execArgs.push_back(opts.input().string());
  std::transform(args.begin(), args.end(), std::back_inserter(execArgs),
                 toString);

  std::string outPath = std::string(kTracePathEnvVar) + "=" + opts.output().c_str();

  std::string ldLibraryPath = "LD_LIBRARY_PATH=";
  ldLibraryPath += (opts.location() / ".." / "lib").string() + ":";
  ldLibraryPath += std::string(std::getenv("LD_LIBRARY_PATH"));

  std::vector<std::string> env;
  std::transform(opts.env().begin(), opts.env().end(), std::back_inserter(env),
                 toString);
  env.emplace_back("XPTI_TRACE_ENABLE=1");
  env.emplace_back("XPTI_FRAMEWORK_DISPATCHER=libxptifw.so");
  env.emplace_back("LD_PRELOAD=libsystem_intercept.so");
  env.emplace_back("XPTI_SUBSCRIBERS=libgraph_dump.so,libplugin_record.so");
  env.push_back(ldLibraryPath.c_str());
  env.push_back(outPath.c_str());

  std::string skipVal = kSkipMemObjsEnvVar;
  skipVal += "=1";
  if (opts.record_skip_mem_objects()) {
    env.push_back(skipVal);
  }

  dpcpp_trace::NativeTracer tracer;

  json files;

  tracer.onFileOpen(
      [&files](std::string_view fileName, const dpcpp_trace::OpenHandler &h) {
        files.push_back(fileName);
      });

  // exit_code c = exec(executable, execArgs, env, tracer);
  tracer.launch(executable, execArgs, env);

  std::ofstream filesOut{opts.output() / kFilesConfigName};
  filesOut << files.dump(4);
  filesOut.close();
}
