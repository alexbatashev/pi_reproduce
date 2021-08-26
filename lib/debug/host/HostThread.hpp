#pragma once

#include "HostRegisterContext.hpp"

#include <lldb/Target/StopInfo.h>
#include <lldb/Target/Thread.h>

using namespace lldb_private;
using namespace lldb;

namespace dpcpp_trace {
class HostThread : public lldb_private::Thread {
public:
  HostThread(Process &proc, tid_t tid, NativeThreadProtocol *thread)
      : Thread(proc, tid), mThread(thread) {}

  void RefreshStateAfterStop() final {}

  RegisterContextSP GetRegisterContext() final {
    if (!mRegContext)
      mRegContext = CreateRegisterContextForFrame(nullptr);
    return mRegContext;
  }

  RegisterContextSP CreateRegisterContextForFrame(StackFrame *frame) final {
    uint32_t concreteFrameIdx = 0;

    // TODO support other cases
    return std::make_shared<HostRegisterContext>(*this, concreteFrameIdx,
                                                 mThread->GetRegisterContext());
  }

  bool CalculateStopInfo() final {
    ThreadStopInfo info;
    std::string description;

    if (!mThread->GetStopReason(info, description))
      return false;

    if (info.reason == eStopReasonSignal) {
      SetStopInfo(StopInfo::CreateStopReasonWithSignal(
          *this, info.details.signal.signo, description.c_str()));
      return true;
    } else if (info.reason == eStopReasonException) {
      SetStopInfo(
          StopInfo::CreateStopReasonWithException(*this, description.c_str()));
      return true;
    }
    return false;
  }

  int GetUnixSignal() {
    ThreadStopInfo info;
    std::string descr;
    mThread->GetStopReason(info, descr);

    return info.details.signal.signo;
  }

private:
  NativeThreadProtocol *mThread;
  RegisterContextSP mRegContext;
};
} // namespace dpcpp_trace
