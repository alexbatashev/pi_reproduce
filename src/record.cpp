#include "common.hpp"
#include "constants.hpp"
#include "fork.hpp"
#include "trace.hpp"

#include <cstdlib>
#include <dlfcn.h>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
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
    std::cerr << "Output path " << opts.output() << " already exists\n";
    std::terminate();
  }
  if (!std::filesystem::create_directory(opts.output())) {
    std::cerr << "Failed to create directory " << opts.output() << "\n";
    std::terminate();
  }

  std::ofstream env{opts.output() / "env"};
  for (auto entry : opts.env()) {
    env << entry << "\n";
  }
  env.close();

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

    replayConfig[kReplayArguments] = json::array();
    for (const auto &arg : args) {
      replayConfig[kReplayArguments].push_back(arg);
    }

    replayConfigFile << replayConfig.dump(4);
    replayConfigFile.close();
  }


  const char *cArgs[args.size() + 2];

  int i = 0;
  cArgs[i++] = opts.input().c_str();
  for (auto &arg : args) {
    cArgs[i++] = arg.c_str();
  }
  cArgs[i] = nullptr;

  std::string outPath = std::string(kTracePathEnvVar) + "=" + opts.output().c_str();

  std::string ldLibraryPath = "LD_LIBRARY_PATH=";
  ldLibraryPath += (opts.location() / ".." / "lib").string() + ":";
  ldLibraryPath +=
      (opts.location() / ".." / "lib" / "plugin_replay").string() + ":";
  ldLibraryPath +=
      (opts.location() / ".." / "lib" / "plugin_record").string() + ":";
  ldLibraryPath +=
      (opts.location() / ".." / "lib" / "system_intercept").string() + ":";
  ldLibraryPath +=
      (opts.location() / ".." / "lib" / "graph_dump").string() + ":";
  ldLibraryPath += std::string(std::getenv("LD_LIBRARY_PATH"));

  std::vector<const char *> cEnv;
  cEnv.reserve(opts.env().size() + 7);
  for (auto e : opts.env()) {
    if (e.find("LD_LIBRARY_PATH") == std::string::npos)
      cEnv.push_back(e.data());
  }
  cEnv.push_back("XPTI_TRACE_ENABLE=1");
  cEnv.push_back("XPTI_FRAMEWORK_DISPATCHER=libxptifw.so");
  cEnv.push_back("LD_PRELOAD=libsystem_intercept.so");
  cEnv.push_back(ldLibraryPath.c_str());
  cEnv.push_back("XPTI_SUBSCRIBERS=libgraph_dump.so,libplugin_record.so");
  cEnv.push_back(outPath.c_str());

  std::string skipVal = kSkipMemObjsEnvVar;
  skipVal += "=1";
  if (opts.record_skip_mem_objects()) {
    cEnv.push_back(skipVal.c_str());
  }

  cEnv.push_back(nullptr);

  const auto start = [&]() {
    traceMe();
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
    pid child = fork(start);
    Tracer tracer(child);

    json files;

    tracer.onFileOpen([&files](const std::string &fileName) {
      std::cout << fileName << "\n";
      files.push_back(fileName);
    });

    tracer.run();

    std::ofstream filesOut{opts.output() / kFilesConfigName};
    filesOut << files.dump(4);
    filesOut.close();
  }
}
