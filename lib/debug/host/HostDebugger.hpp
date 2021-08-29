#pragma once

#include "debug/AbstractDebugger.hpp"
#include "utils/RTTI.hpp"
#include "utils/Tracer.hpp"

#include <lldb/Core/Debugger.h>

#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>

class HostDebugger : public dpcpp_trace::AbstractDebugger,
                     public dpcpp_trace::Tracer,
                     public dpcpp_trace::RTTIChild<HostDebugger> {
  using DebuggerRTTI = dpcpp_trace::RTTIChild<dpcpp_trace::AbstractDebugger>;
  using TracerRTTI = dpcpp_trace::RTTIChild<dpcpp_trace::Tracer>;
  using ThisRTTI = dpcpp_trace::RTTIChild<HostDebugger>;

public:
  constexpr static size_t ID =
      static_cast<size_t>(dpcpp_trace::RTTIHierarchy::HostDebugger);

  HostDebugger();
  ~HostDebugger() final;

  void attach(uint64_t) final;
  void detach() final;

  void launch(std::string_view executable, std::span<std::string> args,
              std::span<std::string> env) final;

  std::vector<uint8_t> getRegistersData(size_t threadId) final;
  void writeRegistersData(std::span<uint8_t> data, uint64_t tid) final;
  std::vector<uint8_t> readRegister(size_t threadIdx, size_t regNum) final;

  void onFileOpen(dpcpp_trace::Tracer::onFileOpenHandler handler) final{};
  void onStat(dpcpp_trace::Tracer::onStatHandler handler) final{};

  bool isAttached() final;

  std::string getGDBTargetXML() final;
  dpcpp_trace::ProcessInfo getProcessInfo() final;
  std::string getExecutablePath() final;

  void createSoftwareBreakpoint(uint64_t address) final;
  void removeSoftwareBreakpoint(uint64_t address) final;
  void resume(int signal, uint64_t tid) final;
  void stepInstruction(uint64_t tid, int signal) final;

  void start() final;
  int wait() final;
  void kill() final;
  void interrupt() final;

  dpcpp_trace::StopReason getStopReason(size_t threadId);

  size_t getNumThreads() final;
  uint64_t getThreadIDAtIndex(size_t threadIdx) final;

  std::vector<uint8_t> readMemory(uint64_t addr, size_t len) final;
  void writeMemory(uint64_t addr, size_t len, std::span<uint8_t> data) final;

  std::vector<uint8_t> getAuxvData() final;

  void *cast(std::size_t type) override;

private:
  void resumeNewPlan(lldb_private::ExecutionContext &ctx,
                     lldb_private::ThreadPlan *newPlan);

  lldb::DebuggerSP mDebugger;
  lldb::TargetSP mTarget;
  lldb::ModuleSP mModule;

  std::string mTargetXML;
  std::string mExecutablePath;
  std::unordered_map<uint64_t, lldb::BreakpointSP> mBreakpoints;
};
