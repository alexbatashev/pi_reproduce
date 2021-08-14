#include "HostDebugger.hpp"
#include "HostProcess.hpp"

#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Host/MainLoop.h"
#include "lldb/Initialization/SystemInitializerCommon.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/ProcessTrace.h"
#include "llvm/Support/TargetSelect.h"

#define LLDB_PLUGIN(p) LLDB_PLUGIN_DECLARE(p)
#include "LLDBPlugins.def"
#undef LLDB_PLUGIN

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace lldb;
using namespace lldb_private;

std::unique_ptr<SystemInitializerCommon> gSystemInitializer;

extern "C" int initialize(DebuggerInfo *) {
  // TODO enable python?
  gSystemInitializer = std::make_unique<SystemInitializerCommon>(nullptr);
  auto err = gSystemInitializer->Initialize();
  if (err) {
    llvm::errs() << err << "\n";
    exit(EXIT_FAILURE);
  }

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetDisassembler();

#define LLDB_PLUGIN(x) LLDB_PLUGIN_INITIALIZE(x);
#include "LLDBPlugins.def"

  dpcpp_trace::HostProcess::Initialize();

  ProcessTrace::Initialize();
  PluginManager::Initialize();

  Debugger::SettingsInitialize();

  return 0;
}

HostDebugger::HostDebugger() {
  mDebugger = Debugger::CreateInstance();
  mDebugger->SetAsyncExecution(true);
}

HostDebugger::~HostDebugger() {
  if (mTarget)
    mTarget->Destroy();
  if (mDebugger)
    Debugger::Destroy(mDebugger);
}

void HostDebugger::launch(std::string_view executable,
                          std::span<std::string> args,
                          std::span<std::string> env) {
  auto error = mDebugger->GetTargetList().CreateTarget(
      *mDebugger, "", "", eLoadDependentsNo, nullptr, mTarget);
  if (error.Fail()) {
    std::terminate();
  }
  ProcessLaunchInfo launchInfo;
  launchInfo.SetExecutableFile(FileSpec(executable.data()), false);
  launchInfo.GetFlags().Set(eLaunchFlagDebug);
  launchInfo.SetLaunchInSeparateProcessGroup(true);

  const auto toCString = [](const std::string &str) { return str.c_str(); };

  std::vector<const char *> cArgs;
  cArgs.reserve(args.size() + 1);
  std::transform(args.begin(), args.end(), std::back_inserter(cArgs),
                 toCString);
  cArgs.push_back(nullptr);

  std::vector<const char *> cEnv;
  cEnv.reserve(env.size());
  std::transform(env.begin(), env.end(), std::back_inserter(cEnv), toCString);
  cEnv.push_back(nullptr);

  launchInfo.SetArguments(cArgs.data(), true);

  launchInfo.SetProcessPluginName("native-host");

  ModuleSpec moduleSpec(FileSpec(executable.data()));
  mModule = mTarget->GetOrCreateModule(moduleSpec, true);
  if (!mModule) {
    std::cerr << "Failed to create module\n";
    std::terminate();
  }

  if (Module *exeModule = mTarget->GetExecutableModulePointer())
    launchInfo.SetExecutableFile(exeModule->GetPlatformFileSpec(), true);

  error = mTarget->Launch(launchInfo, nullptr);
  if (error.Fail()) {
    std::cerr << error.AsCString() << std::endl;
    std::terminate();
  }
}

void HostDebugger::attach(uint64_t pid) {}

void HostDebugger::detach() {
  // mProcess->Detach(false);
}

void HostDebugger::wait() {
  StateType type = eStateInvalid;
  do {
    type = mTarget->GetProcessSP()->WaitForProcessToStop(llvm::None);
    std::cout << "Type is " << (int)type << "\n";
  } while (type != eStateExited || type != eStateCrashed);
}
void HostDebugger::kill() {}
void HostDebugger::interrupt() {}
