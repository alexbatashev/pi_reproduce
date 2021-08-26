#include "HostProcess.hpp"
#include "HostThread.hpp"

#include <exception>
#include <lldb/Core/Debugger.h>
#include <lldb/Core/PluginManager.h>
#include <lldb/Host/MainLoop.h>
#include <lldb/Interpreter/CommandObjectMultiword.h>

#include <iostream>
#include <memory>

#if defined(__linux__)
#include "Plugins/Process/Linux/NativeProcessLinux.h"
#elif defined(__FreeBSD__)
#include "Plugins/Process/FreeBSD/NativeProcessFreeBSD.h"
#elif defined(__NetBSD__)
#include "Plugins/Process/NetBSD/NativeProcessNetBSD.h"
#elif defined(_WIN32)
#include "Plugins/Process/Windows/Common/NativeProcessWindows.h"
#endif

using namespace lldb_private;
using namespace lldb;

#if defined(__linux__)
using NativeProcessFactory = process_linux::NativeProcessLinux::Factory;
#elif defined(_WIN32)
using NativeProcessFactory = NativeProcessWindows::Factory;
#endif

namespace dpcpp_trace {

class HostDelegate
    : public lldb_private::NativeProcessProtocol::NativeDelegate {
public:
  HostDelegate(std::shared_ptr<HostProcess> proc) : mProcess(std::move(proc)) {}
  ~HostDelegate() final = default;

  void InitializeDelegate(lldb_private::NativeProcessProtocol *process) final {}

  void ProcessStateChanged(lldb_private::NativeProcessProtocol *process,
                           lldb::StateType state) final {
    std::cout << "Process state changed: " << state << "\n";
    mProcess->OnProcessStateChanged(state);
  }

  void DidExec(lldb_private::NativeProcessProtocol *process) final {
    std::cout << "Did exec\n";
  }

  void NewSubprocess(lldb_private::NativeProcessProtocol *parent_process,
                     std::unique_ptr<lldb_private::NativeProcessProtocol>
                         child_process) final {
    std::cout << "new subprocess\n";
  }

private:
  std::shared_ptr<HostProcess> mProcess;
};

class PluginProperties : public Properties {
public:
  static ConstString GetSettingName() {
    return HostProcess::GetPluginNameStatic();
  }

  PluginProperties() : Properties() {}

  ~PluginProperties() override = default;
};

using ProcessPropertiesSP = std::shared_ptr<PluginProperties>;

static const ProcessPropertiesSP &GetGlobalPluginProperties() {
  static ProcessPropertiesSP settings;
  if (!settings)
    settings = std::make_shared<PluginProperties>();
  return settings;
}

ConstString HostProcess::GetPluginNameStatic() {
  static ConstString name("native-host");
  return name;
}

const char *HostProcess::GetPluginDescriptionStatic() {
  return "Native Host target plug-in.";
}

void HostProcess::Terminate() {
  PluginManager::UnregisterPlugin(HostProcess::CreateInstance);
}

ProcessSP HostProcess::CreateInstance(TargetSP target, ListenerSP listener,
                                      const FileSpec *crashFilePath,
                                      bool canConnect) {
  ProcessSP proc = nullptr;
  if (crashFilePath == nullptr)
    proc = std::make_shared<HostProcess>(target, listener);
  return proc;
}

void HostProcess::DebuggerInitialize(Debugger &debugger) {
  if (!PluginManager::GetSettingForProcessPlugin(
          debugger, PluginProperties::GetSettingName())) {
    const bool is_global_setting = true;
    PluginManager::CreateSettingForProcessPlugin(
        debugger, GetGlobalPluginProperties()->GetValueProperties(),
        ConstString("Properties for the native-host process plug-in."),
        is_global_setting);
  }
}

void HostProcess::Initialize() {
  static llvm::once_flag onceFlag;

  llvm::call_once(onceFlag, []() {
    lldb_private::PluginManager::RegisterPlugin(
        GetPluginNameStatic(), GetPluginDescriptionStatic(), CreateInstance,
        DebuggerInitialize);
  });
}

HostProcess::HostProcess(lldb::TargetSP target, lldb::ListenerSP listener)
    : Process(target, listener) {}

bool HostProcess::CanDebug(lldb::TargetSP, bool pluginSpecifiedByName) {
  return pluginSpecifiedByName;
}

CommandObject *HostProcess::GetPluginCommandObject() {
  if (!mCommandObject) {
    mCommandObject = std::make_shared<CommandObjectMultiword>(
        GetTarget().GetDebugger().GetCommandInterpreter(),
        "host process plugin");
  }
  return mCommandObject.get();
}

Status HostProcess::WillLaunch(Module *module) {
  // NOP for now
  return Status();
}

Status HostProcess::DoLaunch(Module *exeModule, ProcessLaunchInfo &launchInfo) {
  mDelegate = std::make_unique<HostDelegate>(
      std::static_pointer_cast<HostProcess>(shared_from_this()));
  NativeProcessFactory factory;
  mMainLoop = std::make_shared<MainLoop>();

  auto procOrError = factory.Launch(launchInfo, *mDelegate, *mMainLoop);
  if (!procOrError)
    return Status("Failed to launch process %s", "todo proc name");

  mProcess = std::move(procOrError.get());
  auto *nativeThread = mProcess->GetThreadAtIndex(0);
  if (!nativeThread) {
    std::cerr << "No main thread\n";
    std::terminate();
  }

  SetID(mProcess->GetID());
  SetPrivateState(mProcess->GetState());

  return Status();
}

void HostProcess::DidLaunch() {
  // NOP
}

Status HostProcess::DoDestroy() {
  // NOP for now
  return Status();
}

void HostProcess::RefreshStateAfterStop() {
  SetPrivateState(mProcess->GetState());
}

size_t HostProcess::DoReadMemory(lldb::addr_t addr, void *buf, size_t size,
                                 Status &error) {
  size_t bytesRead;
  error = mProcess->ReadMemoryWithoutTrap(addr, buf, size, bytesRead);

  return bytesRead;
}

bool HostProcess::DoUpdateThreadList(ThreadList &oldThreadList,
                                     ThreadList &newThreadList) {
  size_t newThreadCount = mProcess->UpdateThreads();
  if (newThreadCount == 0)
    return false;

  ThreadList oldListCopy(oldThreadList);
  for (size_t i = 0; i < newThreadCount; i++) {
    auto *nativeThread = mProcess->GetThreadAtIndex(i);
    auto tid = nativeThread->GetID();
    ThreadSP thread(oldListCopy.RemoveThreadByProtocolID(tid, false));
    if (!thread) {
      thread = std::make_shared<HostThread>(*this, tid, nativeThread);
    }

    newThreadList.AddThreadSortedByIndexID(thread);
  }

  return true;
}

HostProcess::~HostProcess() {}

lldb_private::ConstString HostProcess::GetPluginName() {
  return GetPluginNameStatic();
}

lldb_private::DataExtractor HostProcess::GetAuxvData() {
  auto data = mProcess->GetAuxvData();
  if (!data)
    return {};

  return lldb_private::DataExtractor((*data)->getBufferStart(),
                                     (*data)->getBufferSize(), eByteOrderLittle,
                                     sizeof(size_t));
}

Status HostProcess::DoResume() {
  Status err = mProcess->Resume(ResumeActionList());
  if (err.Fail())
    return err;
  SetPrivateState(eStateRunning);
  return Status();
}

void HostProcess::OnProcessStateChanged(lldb::StateType type) {
  SetPrivateState(type);
}
uint32_t HostProcess::GetPluginVersion() { return 0; }
} // namespace dpcpp_trace
