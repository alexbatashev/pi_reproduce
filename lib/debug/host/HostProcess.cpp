#include "HostProcess.hpp"

#include <lldb/Core/PluginManager.h>

using namespace lldb_private;
using namespace lldb;

namespace dpcpp_trace {
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

lldb::ProcessSP HostProcess::CreateInstance(TargetSP target_sp,
                                            ListenerSP listener_sp,
                                            const FileSpec *crash_file_path,
                                            bool can_connect) {
  lldb::ProcessSP proc;
  if (crash_file_path == nullptr)
    proc = std::make_shared<HostProcess>(target_sp, listener_sp);
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
  static llvm::once_flag g_once_flag;

  llvm::call_once(g_once_flag, []() {
    lldb_private::PluginManager::RegisterPlugin(
        GetPluginNameStatic(), GetPluginDescriptionStatic(), CreateInstance,
        DebuggerInitialize);
  });
}

HostProcess::HostProcess(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp)
    : Process(target_sp, listener_sp) {}
} // namespace dpcpp_trace
