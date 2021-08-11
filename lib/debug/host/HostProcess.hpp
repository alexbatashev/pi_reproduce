#include "lldb/Target/Process.h"
#include "lldb/Utility/FileSpec.h"

#include "lldb/lldb-private-forward.h"

namespace dpcpp_trace {
class HostProcess : public lldb_private::Process {
public:
  static void Initialize();
  static lldb_private::ConstString GetPluginNameStatic();
  static const char *GetPluginDescriptionStatic();
  static void Terminate();

  static lldb::ProcessSP
  CreateInstance(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp,
                 const lldb_private::FileSpec *crash_file_path,
                 bool can_connect);
  static void DebuggerInitialize(lldb_private::Debugger &debugger);

  HostProcess(lldb::TargetSP target_sp, lldb::ListenerSP listener_sp);

  ~HostProcess() override;

  lldb_private::ConstString GetPluginName() override;
  uint32_t GetPluginVersion() override;
};
} // namespace dpcpp_trace
