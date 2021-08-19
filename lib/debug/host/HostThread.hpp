#pragma once

#include <lldb/Target/Thread.h>

using namespace lldb_private;
using namespace lldb;

namespace dpcpp_trace {
class HostThread : public lldb_private::Thread {
public:
  HostThread(Process &proc, tid_t tid, NativeThreadProtocol *thread)
      : Thread(proc, tid), mThread(thread) {}

  void RefreshStateAfterStop() final {}

  RegisterContextSP GetRegisterContext() final { return nullptr; }

  RegisterContextSP CreateRegisterContextForFrame(StackFrame *frame) final {
    return nullptr;
  }
  bool CalculateStopInfo() final { return false; }

private:
  NativeThreadProtocol *mThread;
};
} // namespace dpcpp_trace
