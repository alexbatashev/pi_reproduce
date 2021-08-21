#pragma once

#include <lldb/Host/common/NativeRegisterContext.h>
#include <lldb/Target/RegisterContext.h>

using namespace lldb_private;
using namespace lldb;

namespace dpcpp_trace {
class HostRegisterContext : public RegisterContext {
public:
  HostRegisterContext(Thread &thread, uint32_t concreteFrameIdx,
                      NativeRegisterContext &ctx)
      : RegisterContext(thread, concreteFrameIdx), mContext(ctx) {}

  void InvalidateAllRegisters() final {
    // TODO should there be any implementation?
  }

  size_t GetRegisterCount() final { return mContext.GetUserRegisterCount(); }

  const RegisterInfo *GetRegisterInfoAtIndex(size_t reg) final {
    return mContext.GetRegisterInfoAtIndex(reg);
  }

  size_t GetRegisterSetCount() final { return mContext.GetRegisterSetCount(); }

  const RegisterSet *GetRegisterSet(size_t regSet) final {
    return mContext.GetRegisterSet(regSet);
  }

  bool ReadRegister(const RegisterInfo *regInfo, RegisterValue &regVal) final {
    return !mContext.ReadRegister(regInfo, regVal).Fail();
  }

  bool WriteRegister(const RegisterInfo *regInfo,
                     const RegisterValue &regVal) final {
    return !mContext.WriteRegister(regInfo, regVal).Fail();
  }

private:
  NativeRegisterContext &mContext;
};
} // namespace dpcpp_trace
