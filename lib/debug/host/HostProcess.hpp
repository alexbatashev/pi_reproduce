#include <lldb/Host/MainLoop.h>
#include <lldb/Host/common/NativeProcessProtocol.h>
#include <lldb/Interpreter/CommandObject.h>
#include <lldb/Target/Process.h>
#include <lldb/Utility/FileSpec.h>
#include <lldb/Utility/Status.h>

#include <lldb/lldb-private-forward.h>

#include <thread>

namespace dpcpp_trace {
class HostDelegate;

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

  bool CanDebug(lldb::TargetSP target_sp,
                bool plugin_specified_by_name) override;

  lldb_private::CommandObject *GetPluginCommandObject() override;
  lldb_private::Status WillLaunch(lldb_private::Module *module) override;

  lldb_private::Status
  DoLaunch(lldb_private::Module *exe_module,
           lldb_private::ProcessLaunchInfo &launch_info) override;
  void DidLaunch() override;

  ~HostProcess() override;

  lldb_private::Status DoDestroy() override;
  void RefreshStateAfterStop() override;

  size_t DoReadMemory(lldb::addr_t addr, void *buf, size_t size,
                      lldb_private::Status &error) override;

  bool DoUpdateThreadList(lldb_private::ThreadList &old_thread_list,
                          lldb_private::ThreadList &new_thread_list) override;

  lldb_private::ConstString GetPluginName() override;
  uint32_t GetPluginVersion() override;

private:
  std::jthread mWorker;
  std::unique_ptr<lldb_private::NativeProcessProtocol> mProcess;
  std::shared_ptr<lldb_private::MainLoop> mMainLoop;
  std::unique_ptr<HostDelegate> mDelegate;
};
} // namespace dpcpp_trace
