#pragma once

#include "debug/AbstractDebugger.hpp"
#include "utils/Tracer.hpp"

#include <lldb/Core/Debugger.h>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

class HostDebugger : public dpcpp_trace::AbstractDebugger, dpcpp_trace::Tracer {
public:
  HostDebugger();
  ~HostDebugger() final;

  void attach(uint64_t) final;
  void detach() final;

  void launch(std::string_view executable, std::span<std::string> args,
              std::span<std::string> env) final;

  void onFileOpen(dpcpp_trace::Tracer::onFileOpenHandler handler) final{};
  void onStat(dpcpp_trace::Tracer::onStatHandler handler) final{};

  int wait() final;
  void kill() final;
  void interrupt() final;

private:
  lldb::DebuggerSP mDebugger;
  lldb::TargetSP mTarget;
  lldb::ModuleSP mModule;
};
