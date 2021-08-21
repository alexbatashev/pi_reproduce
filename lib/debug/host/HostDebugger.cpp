#include "HostDebugger.hpp"
#include "HostProcess.hpp"

#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Host/MainLoop.h"
#include "lldb/Initialization/SystemInitializerCommon.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/ProcessTrace.h"
#include "lldb/Target/RegisterContext.h"
#include "llvm/Support/TargetSelect.h"
#include <lldb/Utility/RegisterValue.h>

#define LLDB_PLUGIN(p) LLDB_PLUGIN_DECLARE(p)
#include "LLDBPlugins.def"
#undef LLDB_PLUGIN

#include <iostream>
#include <memory>
#include <stdexcept>

using namespace lldb;
using namespace lldb_private;

std::unique_ptr<SystemInitializerCommon> gSystemInitializer;

extern "C" void deinitialize(dpcpp_trace::AbstractDebugger *d) { delete d; }

extern "C" int initialize(DebuggerInfo *info) {
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

  info->debugger = new HostDebugger();
  info->deinitialize = deinitialize;

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
  FileSpec execFileSpec(executable.data());
  launchInfo.SetExecutableFile(execFileSpec, false);
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

  auto ep = mTarget->GetEntryPointAddress();
  if (!ep) {
    std::cerr << "Failed to get entry point in executable\n";
    std::terminate();
  }

  mStartBP = mTarget->CreateAddressInModuleBreakpoint(
      ep->GetFileAddress(), true, &execFileSpec, false);

  error = mTarget->Launch(launchInfo, nullptr);
  if (error.Fail()) {
    std::cerr << error.AsCString() << std::endl;
    std::terminate();
  }
}

std::vector<uint8_t> HostDebugger::getRegistersData(size_t threadId) {
  auto thread =
      mTarget->GetProcessSP()->GetThreadList().GetThreadAtIndex(threadId);
  auto regContext = thread->GetRegisterContext();

  std::vector<uint8_t> buffer;

  for (uint32_t regNum = 0; regNum < regContext->GetRegisterCount(); regNum++) {
    const RegisterInfo *regInfo = regContext->GetRegisterInfoAtIndex(regNum);
    if (regInfo == nullptr) {
      std::cerr << "Failed to get register info\n";
      std::terminate();
    }
    if (regInfo->value_regs != nullptr)
      continue; // skip registers that are contained in other registers
    RegisterValue regValue;
    bool success = regContext->ReadRegister(regInfo, regValue);
    if (!success) {
      std::cerr << "Failed to read register #" << regNum << "\n";
      std::terminate();
    }

    if (regInfo->byte_offset + regInfo->byte_size >= buffer.size())
      buffer.resize(regInfo->byte_offset + regInfo->byte_size);

    memcpy(buffer.data() + regInfo->byte_offset, regValue.GetBytes(),
           regInfo->byte_size);
  }

  return buffer;
}

void HostDebugger::attach(uint64_t pid) {}

void HostDebugger::detach() {
  // mProcess->Detach(false);
}

int HostDebugger::wait() {
  StateType type = eStateInvalid;
  do {
    type = mTarget->GetProcessSP()->WaitForProcessToStop(llvm::None);
    std::cout << "Type is " << (int)type << "\n";
  } while (type != eStateExited || type != eStateCrashed);
  return 0;
}
void HostDebugger::kill() {}
void HostDebugger::interrupt() {}

bool HostDebugger::isAttached() { return mTarget != nullptr; }

void *HostDebugger::cast(size_t type) {
  if (type == DebuggerRTTI::getID()) {
    return static_cast<dpcpp_trace::AbstractDebugger *>(this);
  } else if (type == TracerRTTI::getID()) {
    return static_cast<dpcpp_trace::Tracer *>(this);
  } else if (type == ThisRTTI::getID()) {
    return this;
  }
  return nullptr;
}
