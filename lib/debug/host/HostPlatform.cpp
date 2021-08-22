#include "HostPlatform.hpp"

#include "lldb/Core/Debugger.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Host/Host.h"
#include "lldb/Host/HostInfo.h"
#include "lldb/Host/ProcessLaunchInfo.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/Target.h"

using namespace lldb_private;
using namespace lldb_private::platform_linux;
using namespace lldb;

namespace dpcpp_trace {
HostPlatform::HostPlatform() : PlatformLinux(true) {}

PlatformSP HostPlatform::CreateInstance(bool force, const ArchSpec *arch) {
  if (force)
    return PlatformSP(new HostPlatform());
  return PlatformSP();
}

void HostPlatform::Initialize() {
  PlatformPOSIX::Initialize();

  PlatformSP defaultPlt(new HostPlatform());
  defaultPlt->SetSystemArchitecture(HostInfo::GetArchitecture());
  Platform::SetHostPlatform(defaultPlt);

  PluginManager::RegisterPlugin(HostPlatform::GetPluginNameStatic(),
                                HostPlatform::GetPluginDescriptionStatic(),
                                HostPlatform::CreateInstance, nullptr);
}

void HostPlatform::Terminate() {
  PluginManager::UnregisterPlugin(HostPlatform::CreateInstance);
  PlatformPOSIX::Terminate();
}

ProcessSP HostPlatform::DebugProcess(ProcessLaunchInfo &launchInfo,
                                     Debugger &debugger, Target *target,
                                     Status &error) {
  launchInfo.GetFlags().Set(eLaunchFlagDebug);
  launchInfo.SetLaunchInSeparateProcessGroup(true);

  if (target == nullptr) {
    TargetSP newTarget;
    error = debugger.GetTargetList().CreateTarget(
        debugger, "", "", eLoadDependentsNo, nullptr, newTarget);
    if (error.Fail()) {
      return nullptr;
    }

    target = newTarget.get();
    if (!target) {
      error.SetErrorString("CreateTarget() returned nullptr");
      return nullptr;
    }
  }

  auto process = target->CreateProcess(launchInfo.GetListener(), "native-host",
                                       nullptr, true);

  if (!process) {
    error.SetErrorString("CreateProcess() failed for native-host process");
    return process;
  }

  ListenerSP listener;
  if (!launchInfo.GetHijackListener()) {
    listener =
        Listener::MakeListener("dpcpp_trace.HostPlatform.DebugProcess.hijack");
    launchInfo.SetHijackListener(listener);
    process->HijackProcessEvents(listener);
  }

  error = process->Launch(launchInfo);
  if (error.Success()) {
    if (listener) {
      const StateType state =
          process->WaitForProcessToStop(llvm::None, nullptr, false, listener);
    }

    int pty_fd = launchInfo.GetPTY().ReleasePrimaryFileDescriptor();
    if (pty_fd != PseudoTerminal::invalid_fd)
      process->SetSTDIOFileDescriptor(pty_fd);
  }

  return process;
}
} // namespace dpcpp_trace
