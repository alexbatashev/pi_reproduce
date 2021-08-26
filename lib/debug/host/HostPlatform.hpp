#pragma once

#include <Plugins/Platform/Linux/PlatformLinux.h>
#include <lldb/Target/Platform.h>

namespace dpcpp_trace {
class HostPlatform : public lldb_private::platform_linux::PlatformLinux {
public:
  HostPlatform();

  static void Initialize();

  static void Terminate();

  static lldb::PlatformSP CreateInstance(bool force,
                                         const lldb_private::ArchSpec *arch);

  static lldb_private::ConstString GetPluginNameStatic() {
    return lldb_private::ConstString("host-platform");
  }

  static const char *GetPluginDescriptionStatic() {
    return "Native host platform plugin";
  }

  lldb_private::ConstString GetPluginName() override {
    return GetPluginNameStatic();
  }

  uint32_t GetPluginVersion() override { return 1; }

  const char *GetDescription() override { return GetPluginDescriptionStatic(); }

  bool CanDebugProcess() override { return true; }

  lldb::ProcessSP DebugProcess(lldb_private::ProcessLaunchInfo &launchInfo,
                               lldb_private::Debugger &debugger,
                               lldb_private::Target *target,
                               lldb_private::Status &error) override;
};
} // namespace dpcpp_trace
