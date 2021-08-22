#include "HostDebugger.hpp"
#include "HostPlatform.hpp"
#include "HostProcess.hpp"
#include "HostThread.hpp"

#include "lldb/Core/Module.h"
#include "lldb/Core/PluginManager.h"
#include "lldb/Host/MainLoop.h"
#include "lldb/Initialization/SystemInitializerCommon.h"
#include "lldb/Target/Process.h"
#include "lldb/Target/ProcessTrace.h"
#include "lldb/Target/RegisterContext.h"
#include "lldb/Target/StopInfo.h"
#include "llvm/Support/TargetSelect.h"
#include <cstdint>
#include <lldb/Utility/RegisterValue.h>

#define LLDB_PLUGIN(p) LLDB_PLUGIN_DECLARE(p)
#include "LLDBPlugins.def"
#undef LLDB_PLUGIN

#include <cpuinfo.h>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace lldb;
using namespace lldb_private;
namespace fs = std::filesystem;

std::unique_ptr<SystemInitializerCommon> gSystemInitializer;

extern "C" void deinitialize(dpcpp_trace::AbstractDebugger *d) { delete d; }

extern "C" int initialize(DebuggerInfo *info) {
  // TODO enable python?
  gSystemInitializer = std::make_unique<SystemInitializerCommon>(nullptr);
  auto err = gSystemInitializer->Initialize();
  if (err) {
    llvm::errs() << err << "\n";
    exit(EXIT_FAILURE);
  }

  llvm::InitializeNativeTarget();
  llvm::InitializeNativeTargetAsmPrinter();
  llvm::InitializeNativeTargetDisassembler();

#define LLDB_PLUGIN(x) LLDB_PLUGIN_INITIALIZE(x);
#include "LLDBPlugins.def"

  dpcpp_trace::HostProcess::Initialize();
  dpcpp_trace::HostPlatform::Initialize();

  ProcessTrace::Initialize();
  PluginManager::Initialize();

  Debugger::SettingsInitialize();

  info->debugger = new HostDebugger();
  info->deinitialize = deinitialize;

  return 0;
}

HostDebugger::HostDebugger() {
  mDebugger = Debugger::CreateInstance();
  mDebugger->SetAsyncExecution(true);
}

HostDebugger::~HostDebugger() {
  if (mTarget)
    mTarget->Destroy();
  if (mDebugger)
    Debugger::Destroy(mDebugger);
}

void HostDebugger::launch(std::string_view executable,
                          std::span<std::string> args,
                          std::span<std::string> env) {
  mExecutablePath = fs::canonical(executable).string();

  auto error = mDebugger->GetTargetList().CreateTarget(
      *mDebugger, "", "", eLoadDependentsNo, nullptr, mTarget);
  if (error.Fail()) {
    std::terminate();
  }
  ProcessLaunchInfo launchInfo;
  FileSpec execFileSpec(executable.data());
  launchInfo.SetExecutableFile(execFileSpec, false);
  launchInfo.GetFlags().Set(eLaunchFlagDebug);
  launchInfo.SetLaunchInSeparateProcessGroup(true);

  const auto toCString = [](const std::string &str) { return str.c_str(); };

  std::vector<const char *> cArgs;
  cArgs.reserve(args.size() + 1);
  std::transform(args.begin(), args.end(), std::back_inserter(cArgs),
                 toCString);
  cArgs.push_back(nullptr);

  std::vector<const char *> cEnv;
  cEnv.reserve(env.size());
  std::transform(env.begin(), env.end(), std::back_inserter(cEnv), toCString);
  cEnv.push_back(nullptr);

  launchInfo.SetArguments(cArgs.data(), true);


  ModuleSpec moduleSpec(FileSpec(executable.data()));
  mModule = mTarget->GetOrCreateModule(moduleSpec, true);
  if (!mModule) {
    std::cerr << "Failed to create module\n";
    std::terminate();
  }

  if (Module *exeModule = mTarget->GetExecutableModulePointer())
    launchInfo.SetExecutableFile(exeModule->GetPlatformFileSpec(), true);

  mProcess = Platform::GetHostPlatform()->DebugProcess(launchInfo, *mDebugger,
                                                       mTarget.get(), error);

  if (error.Fail()) {
    std::cerr << "Failed to launch process: " << error.AsCString() << "\n";
    std::terminate();
  }

  if (!mProcess) {
    std::cerr << "Failed to launch process\n";
    std::terminate();
  }
}

std::vector<uint8_t> HostDebugger::getRegistersData(size_t threadId) {
  auto thread =
      mTarget->GetProcessSP()->GetThreadList().GetThreadAtIndex(threadId);
  auto regContext = thread->GetRegisterContext();

  std::vector<uint8_t> buffer;

#if defined(__amd64__)
  constexpr size_t NumGPRegisters = 16;
#endif

  for (uint32_t regNum = 0; regNum < NumGPRegisters; regNum++) {
    const RegisterInfo *regInfo = regContext->GetRegisterInfoAtIndex(regNum);
    if (regInfo == nullptr) {
      return {};
    }
    if (regInfo->value_regs != nullptr)
      continue; // skip registers that are contained in other registers
    RegisterValue regValue;
    bool success = regContext->ReadRegister(regInfo, regValue);
    if (!success) {
      return {};
    }

    if (regInfo->byte_offset + regInfo->byte_size >= buffer.size())
      buffer.resize(regInfo->byte_offset + regInfo->byte_size);

    memcpy(buffer.data() + regInfo->byte_offset, regValue.GetBytes(),
           regInfo->byte_size);
  }

  return buffer;
}

void HostDebugger::attach(uint64_t pid) {}

void HostDebugger::detach() {
  // mProcess->Detach(false);
}

int HostDebugger::wait() {
  StateType type = eStateInvalid;
  do {
    type = mTarget->GetProcessSP()->WaitForProcessToStop(llvm::None);
  } while (type != eStateExited || type != eStateCrashed);
  return 0;
}
void HostDebugger::start() {}
void HostDebugger::kill() {}
void HostDebugger::interrupt() {}

bool HostDebugger::isAttached() { return mTarget != nullptr; }

dpcpp_trace::StopReason HostDebugger::getStopReason(size_t threadId) {
  auto thread =
      mTarget->GetProcessSP()->GetThreadList().GetThreadAtIndex(threadId);

  auto stateType = mTarget->GetProcessSP()->GetState();

  dpcpp_trace::StopReason returnReason;

  if (stateType == eStateExited) {
    returnReason.type = dpcpp_trace::StopReason::Type::exit;
    return returnReason;
  }

  if (stateType == eStateStopped) {
    auto realReason = thread->GetStopReason();
    switch (realReason) {
    case eStopReasonInvalid:
      returnReason.type = dpcpp_trace::StopReason::Type::none;
      break;
    case eStopReasonTrace:
      returnReason.type = dpcpp_trace::StopReason::Type::trace;
      break;
    case eStopReasonNone: // Software breakpoints?
    case eStopReasonBreakpoint:
      returnReason.type = dpcpp_trace::StopReason::Type::breakpoint;
      break;
    case eStopReasonWatchpoint:
      returnReason.type = dpcpp_trace::StopReason::Type::watchpoint;
      break;
    case eStopReasonSignal: {
      returnReason.type = dpcpp_trace::StopReason::Type::signal;
      auto info = thread->GetStopInfo();
      returnReason.code = static_cast<int>(info->GetValue());
      break;
    }
    case eStopReasonException:
      returnReason.type = dpcpp_trace::StopReason::Type::exception;
      break;
    case eStopReasonFork:
      returnReason.type = dpcpp_trace::StopReason::Type::fork;
      break;
    case eStopReasonVFork:
      returnReason.type = dpcpp_trace::StopReason::Type::vfork;
      break;
    case eStopReasonVForkDone:
      returnReason.type = dpcpp_trace::StopReason::Type::vfork_done;
      break;
    }
  }

  return returnReason;
}

size_t HostDebugger::getNumThreads() {
  return mTarget->GetProcessSP()->GetThreadList().GetSize();
}

uint64_t HostDebugger::getThreadIDAtIndex(size_t threadIdx) {
  return mTarget->GetProcessSP()
      ->GetThreadList()
      .GetThreadAtIndex(threadIdx)
      ->GetID();
}

std::vector<uint8_t> HostDebugger::readMemory(uint64_t addr, size_t len) {
  std::vector<uint8_t> buf;
  buf.resize(len);
  Status status;

  auto proc = std::static_pointer_cast<dpcpp_trace::HostProcess>(
      mTarget->GetProcessSP());
  size_t actualBytes =
      proc->ReadMemoryFromInferior(addr, buf.data(), len, status);
  if (status.Fail() && actualBytes == 0) {
    return {};
  }

  buf.resize(actualBytes);

  return buf;
}

std::string HostDebugger::getExecutablePath() { return mExecutablePath; }

inline constexpr auto GDBXMLTemplate = R"(<?xml version="1.0"?>
<!DOCTYPE target SYSTEM "gdb-target.dtd">
<target version="1.0">
<architecture>{}</architecture>
<osabi>{}</osabi>
{}
</target>
)";

// TODO find a portable way of doing the same.
std::string HostDebugger::getGDBTargetXML() {
  if (mTargetXML.empty()) {
#ifdef linux
    constexpr auto arch = "i386:x86-64";
    constexpr auto abi = "GNU/Linux";
#endif

    cpuinfo_initialize();

    std::string features;

    features = R"(<feature name="org.gnu.gdb.i386.core">
 <flags id="i386_eflags" size="4">
 <field name="CF" start="0" end="0" type="bool"/>
 <field name="" start="1" end="1" type="bool"/>
 <field name="PF" start="2" end="2" type="bool"/>
 <field name="AF" start="4" end="4" type="bool"/>
 <field name="ZF" start="6" end="6" type="bool"/>
 <field name="SF" start="7" end="7" type="bool"/>
 <field name="TF" start="8" end="8" type="bool"/>
 <field name="IF" start="9" end="9" type="bool"/>
 <field name="DF" start="10" end="10" type="bool"/>
 <field name="OF" start="11" end="11" type="bool"/>
 <field name="NT" start="14" end="14" type="bool"/>
 <field name="RF" start="16" end="16" type="bool"/>
 <field name="VM" start="17" end="17" type="bool"/>
 <field name="AC" start="18" end="18" type="bool"/>
 <field name="VIF" start="19" end="19" type="bool"/>
 <field name="VIP" start="20" end="20" type="bool"/>
 <field name="ID" start="21" end="21" type="bool"/>
 </flags>
 <reg name="rax" bitsize="64" type="int64" regnum="0"/>
 <reg name="rbx" bitsize="64" type="int64" regnum="1"/>
 <reg name="rcx" bitsize="64" type="int64" regnum="2"/>
 <reg name="rdx" bitsize="64" type="int64" regnum="3"/>
 <reg name="rsi" bitsize="64" type="int64" regnum="4"/>
 <reg name="rdi" bitsize="64" type="int64" regnum="5"/>
 <reg name="rbp" bitsize="64" type="data_ptr" regnum="6"/>
 <reg name="rsp" bitsize="64" type="data_ptr" regnum="7"/>
 <reg name="r8" bitsize="64" type="int64" regnum="8"/>
 <reg name="r9" bitsize="64" type="int64" regnum="9"/>
 <reg name="r10" bitsize="64" type="int64" regnum="10"/>
 <reg name="r11" bitsize="64" type="int64" regnum="11"/>
 <reg name="r12" bitsize="64" type="int64" regnum="12"/>
 <reg name="r13" bitsize="64" type="int64" regnum="13"/>
 <reg name="r14" bitsize="64" type="int64" regnum="14"/>
 <reg name="r15" bitsize="64" type="int64" regnum="15"/>
 <reg name="rip" bitsize="64" type="code_ptr" regnum="16"/>
 <reg name="eflags" bitsize="32" type="i386_eflags" regnum="17"/>
 <reg name="cs" bitsize="32" type="int32" regnum="18"/>
 <reg name="ss" bitsize="32" type="int32" regnum="19"/>
 <reg name="ds" bitsize="32" type="int32" regnum="20"/>
 <reg name="es" bitsize="32" type="int32" regnum="21"/>
 <reg name="fs" bitsize="32" type="int32" regnum="22"/>
 <reg name="gs" bitsize="32" type="int32" regnum="23"/>
 <reg name="st0" bitsize="80" type="i387_ext" regnum="24"/>
 <reg name="st1" bitsize="80" type="i387_ext" regnum="25"/>
 <reg name="st2" bitsize="80" type="i387_ext" regnum="26"/>
 <reg name="st3" bitsize="80" type="i387_ext" regnum="27"/>
 <reg name="st4" bitsize="80" type="i387_ext" regnum="28"/>
 <reg name="st5" bitsize="80" type="i387_ext" regnum="29"/>
 <reg name="st6" bitsize="80" type="i387_ext" regnum="30"/>
 <reg name="st7" bitsize="80" type="i387_ext" regnum="31"/>
 <reg name="fctrl" bitsize="32" type="int" regnum="32" group="float"/>
 <reg name="fstat" bitsize="32" type="int" regnum="33" group="float"/>
 <reg name="ftag" bitsize="32" type="int" regnum="34" group="float"/>
 <reg name="fiseg" bitsize="32" type="int" regnum="35" group="float"/>
 <reg name="fioff" bitsize="32" type="int" regnum="36" group="float"/>
 <reg name="foseg" bitsize="32" type="int" regnum="37" group="float"/>
 <reg name="fooff" bitsize="32" type="int" regnum="38" group="float"/>
 <reg name="fop" bitsize="32" type="int" regnum="39" group="float"/>
</feature>
<feature name="org.gnu.gdb.i386.linux">
  <reg name="orig_rax" bitsize="64" type="int" regnum="57"/>
</feature>
<feature name="org.gnu.gdb.i386.segments">
  <reg name="fs_base" bitsize="64" type="int" regnum="58"/>
  <reg name="gs_base" bitsize="64" type="int" regnum="59"/>
</feature>
    )";

    if (cpuinfo_has_x86_sse()) {
      features += R"(<feature name="org.gnu.gdb.i386.sse">
 <vector id="v8bf16" type="bfloat16" count="8"/>
 <vector id="v4f" type="ieee_single" count="4"/>
 <vector id="v2d" type="ieee_double" count="2"/>
 <vector id="v16i8" type="int8" count="16"/>
 <vector id="v8i16" type="int16" count="8"/>
 <vector id="v4i32" type="int32" count="4"/>
 <vector id="v2i64" type="int64" count="2"/>
 <union id="vec128">
 <field name="v8_bfloat16" type="v8bf16"/>
 <field name="v4_float" type="v4f"/>
 <field name="v2_double" type="v2d"/>
 <field name="v16_int8" type="v16i8"/>
 <field name="v8_int16" type="v8i16"/>
 <field name="v4_int32" type="v4i32"/>
 <field name="v2_int64" type="v2i64"/>
 <field name="uint128" type="uint128"/>
 </union>
 <flags id="i386_mxcsr" size="4">
 <field name="IE" start="0" end="0" type="bool"/>
 <field name="DE" start="1" end="1" type="bool"/> <field name="ZE" start="2" end="2" type="bool"/>
 <field name="OE" start="3" end="3" type="bool"/>
 <field name="UE" start="4" end="4" type="bool"/>
 <field name="PE" start="5" end="5" type="bool"/>
 <field name="DAZ" start="6" end="6" type="bool"/>
 <field name="IM" start="7" end="7" type="bool"/>
 <field name="DM" start="8" end="8" type="bool"/>
 <field name="ZM" start="9" end="9" type="bool"/>
 <field name="OM" start="10" end="10" type="bool"/>
 <field name="UM" start="11" end="11" type="bool"/>
 <field name="PM" start="12" end="12" type="bool"/>
 <field name="FZ" start="15" end="15" type="bool"/>
 </flags>
 <reg name="xmm0" bitsize="128" type="vec128" regnum="40"/>
 <reg name="xmm1" bitsize="128" type="vec128" regnum="41"/>
 <reg name="xmm2" bitsize="128" type="vec128" regnum="42"/>
 <reg name="xmm3" bitsize="128" type="vec128" regnum="43"/>
 <reg name="xmm4" bitsize="128" type="vec128" regnum="44"/>
 <reg name="xmm5" bitsize="128" type="vec128" regnum="45"/>
 <reg name="xmm6" bitsize="128" type="vec128" regnum="46"/>
 <reg name="xmm7" bitsize="128" type="vec128" regnum="47"/>
 <reg name="xmm8" bitsize="128" type="vec128" regnum="48"/>
 <reg name="xmm9" bitsize="128" type="vec128" regnum="49"/>
 <reg name="xmm10" bitsize="128" type="vec128" regnum="50"/>
 <reg name="xmm11" bitsize="128" type="vec128" regnum="51"/>
 <reg name="xmm12" bitsize="128" type="vec128" regnum="52"/>
 <reg name="xmm13" bitsize="128" type="vec128" regnum="53"/>
 <reg name="xmm14" bitsize="128" type="vec128" regnum="54"/>
 <reg name="xmm15" bitsize="128" type="vec128" regnum="55"/>
 <reg name="mxcsr" bitsize="32" type="i386_mxcsr" regnum="56" group="vector"/>
  </feature>
)";
    }

    if (cpuinfo_has_x86_avx()) {
      features += R"(<feature name="org.gnu.gdb.i386.avx">
 <reg name="ymm0h" bitsize="128" type="uint128" regnum="60"/>
 <reg name="ymm1h" bitsize="128" type="uint128" regnum="61"/>
 <reg name="ymm2h" bitsize="128" type="uint128" regnum="62"/>
 <reg name="ymm3h" bitsize="128" type="uint128" regnum="63"/>
 <reg name="ymm4h" bitsize="128" type="uint128" regnum="64"/>
 <reg name="ymm5h" bitsize="128" type="uint128" regnum="65"/>
 <reg name="ymm6h" bitsize="128" type="uint128" regnum="66"/>
 <reg name="ymm7h" bitsize="128" type="uint128" regnum="67"/>
 <reg name="ymm8h" bitsize="128" type="uint128" regnum="68"/>
 <reg name="ymm9h" bitsize="128" type="uint128" regnum="69"/>
 <reg name="ymm10h" bitsize="128" type="uint128" regnum="70"/>
 <reg name="ymm11h" bitsize="128" type="uint128" regnum="71"/>
 <reg name="ymm12h" bitsize="128" type="uint128" regnum="72"/>
 <reg name="ymm13h" bitsize="128" type="uint128" regnum="73"/>
 <reg name="ymm14h" bitsize="128" type="uint128" regnum="74"/>
 <reg name="ymm15h" bitsize="128" type="uint128" regnum="75"/>
</feature>
)";
    }

    mTargetXML = fmt::format(GDBXMLTemplate, arch, abi, features);
  }

  return mTargetXML;
}

void HostDebugger::CreateSWBreakpoint(uint64_t address) {
  mTarget->CreateBreakpoint(address, false, false);
}

void HostDebugger::resume(int signal, uint64_t tid) {
  if (signal > 0) {
    auto thread = mProcess->GetThreadList().FindThreadByID(tid);
    if (thread)
      thread->SetResumeSignal(signal);
  }
  StateType type = mProcess->GetState();
  if (type == eStateRunning)
    return;
  auto err = mProcess->Resume();
  if (err.Fail()) {
    std::clog << "Failed to resume process " << err.AsCString() << "\n";
    return;
  }
  type = mProcess->GetState();
  while (type != eStateStopped && type != eStateExited &&
         type != eStateCrashed) {
    type = mTarget->GetProcessSP()->WaitForProcessToStop(llvm::None);
  }
}

std::vector<uint8_t> HostDebugger::getAuxvData() {
  auto proc = mTarget->GetProcessSP();
  auto data = proc->GetAuxvData();

  std::vector<uint8_t> ret;
  ret.resize(data.GetByteSize());
  data.CopyData(0, data.GetByteSize(), ret.data());

  return ret;
}

void *HostDebugger::cast(size_t type) {
  if (type == DebuggerRTTI::getID()) {
    return static_cast<dpcpp_trace::AbstractDebugger *>(this);
  } else if (type == TracerRTTI::getID()) {
    return static_cast<dpcpp_trace::Tracer *>(this);
  } else if (type == ThisRTTI::getID()) {
    return this;
  }
  return nullptr;
}
