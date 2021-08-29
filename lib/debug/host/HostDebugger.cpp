#include "HostDebugger.hpp"

#include <Acceptor.h>
#include <Plugins/Process/gdb-remote/GDBRemoteCommunicationServerLLGS.h>
#include <lldb/Core/Module.h>
#include <lldb/Core/PluginManager.h>
#include <lldb/Host/ConnectionFileDescriptor.h>
#include <lldb/Host/MainLoop.h>
#include <lldb/Host/common/NativeProcessProtocol.h>
#include <lldb/Host/common/TCPSocket.h>
#include <lldb/Initialization/SystemInitializerCommon.h>
#include <lldb/Target/ExecutionContext.h>
#include <lldb/Target/Process.h>
#include <lldb/Target/ProcessTrace.h>
#include <lldb/Target/RegisterContext.h>
#include <lldb/Target/StopInfo.h>
#include <lldb/Target/ThreadPlan.h>
#include <lldb/Utility/ProcessInfo.h>
#include <lldb/Utility/RegisterValue.h>
#include <llvm/Support/TargetSelect.h>

#if defined(__linux__)
#include <Plugins/Process/Linux/NativeProcessLinux.h>
#elif defined(_WIN32)
#include <Plugins/Process/Windows/Common/NativeProcessWindows.h>
#endif

#define LLDB_PLUGIN(p) LLDB_PLUGIN_DECLARE(p)
#include "LLDBPlugins.def"
#undef LLDB_PLUGIN

#include <cpuinfo.h>
#include <cstdint>
#include <exception>
#include <fcntl.h>
#include <filesystem>
#include <fmt/core.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using namespace lldb;
using namespace lldb_private;
using namespace lldb_private::lldb_server;
namespace fs = std::filesystem;

#if defined(__linux__)
typedef process_linux::NativeProcessLinux::Factory NativeProcessFactory;
#elif defined(_WIN32)
typedef NativeProcessWindows::Factory NativeProcessFactory;
#endif

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

  ProcessLaunchInfo launchInfo = mTarget->GetProcessLaunchInfo();
  FileSpec execFileSpec(executable.data());
  launchInfo.SetExecutableFile(execFileSpec, false);
  launchInfo.GetFlags().Set(eLaunchFlagDebug | eLaunchFlagStopAtEntry);
  launchInfo.SetLaunchInSeparateProcessGroup(true);

  const auto toCString = [](const std::string &str) { return str.c_str(); };

  std::vector<const char *> cArgs;
  cArgs.reserve(args.size());
  std::transform(args.begin(), args.end(), std::back_inserter(cArgs),
                 toCString);
  cArgs.push_back(nullptr);

  std::vector<const char *> cEnv;
  cEnv.reserve(env.size());
  std::transform(env.begin(), env.end(), std::back_inserter(cEnv), toCString);
  cEnv.push_back(nullptr);

  launchInfo.SetArguments(cArgs.data(), true);
  launchInfo.SetArg0(args.front());

  ::pid_t pid = ::fork();

  if (pid == 0) {
    MainLoop mainLoop;
    NativeProcessFactory factory;
    process_gdb_remote::GDBRemoteCommunicationServerLLGS server(mainLoop,
                                                                factory);

    server.SetLaunchInfo(launchInfo);
    error = server.LaunchProcess();

    if (error.Fail()) {
      std::cerr << "Failed to launch process: " << error.AsCString() << "\n";
      std::terminate();
    }

    auto listenerOrError = Socket::TcpListen("localhost:11111", true, nullptr);
    if (!listenerOrError)
      std::terminate();

    auto listener = std::move(*listenerOrError);

    Socket *connSocket;
    error = listener->Accept(connSocket);

    if (error.Fail()) {
      std::cerr << "Failed to accept connection: " << error.AsCString() << "\n";
      std::terminate();
    }

    std::unique_ptr<Connection> connection(
        new ConnectionFileDescriptor(connSocket));

    error = server.InitializeConnection(std::move(connection));

    if (error.Fail()) {
      std::cerr << "Failed to initialize connection: " << error.AsCString()
                << "\n";
      std::terminate();
    }

    if (!server.IsConnected()) {
      std::cerr << "gdb-remote is not connected\n";
      std::terminate();
    }

    error = mainLoop.Run();
    if (error.Fail()) {
      std::cerr << "Failed to start main loop: " << error.AsCString() << "\n";
      std::terminate();
    }
    exit(0);
  }

  ModuleSpec moduleSpec(FileSpec(executable.data()));
  mModule = mTarget->GetOrCreateModule(moduleSpec, true);
  if (!mModule) {
    std::cerr << "Failed to create module\n";
    std::terminate();
  }

  auto proc = mTarget->CreateProcess(nullptr, "gdb-remote", nullptr, true);

  if (!proc) {
    std::cerr << "Failed to create gdb-remote process\n";
    std::terminate();
  }

  error = proc->ConnectRemote("tcp-connect://localhost:11111");
  if (error.Fail()) {
    std::cerr << "Failed to connect to gdb-remote process: "
              << error.AsCString() << "\n";
    std::terminate();
  }

  proc->WaitForProcessToStop(llvm::None);
}

std::vector<uint8_t> HostDebugger::getRegistersData(size_t threadId) {
  auto thread =
      mTarget->GetProcessSP()->GetThreadList().FindThreadByID(threadId);
  if (threadId == 0)
    thread = mTarget->GetProcessSP()->GetThreadList().GetThreadAtIndex(0);

  auto regContext = thread->GetRegisterContext();

  std::vector<uint8_t> buffer;

  for (uint32_t regNum = 0; regNum < regContext->GetRegisterCount(); regNum++) {
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

std::vector<uint8_t> HostDebugger::readRegister(size_t threadId, size_t regNum) {
  auto thread =
      mTarget->GetProcessSP()->GetThreadList().FindThreadByID(threadId);
  if (threadId == 0)
    thread = mTarget->GetProcessSP()->GetThreadList().GetThreadAtIndex(0);

  auto regContext = thread->GetRegisterContext();

  if (regNum > regContext->GetRegisterCount())
    return {};

  std::vector<uint8_t> buffer;

  const RegisterInfo *regInfo = regContext->GetRegisterInfoAtIndex(regNum);
  RegisterValue regValue;
  bool success = regContext->ReadRegister(regInfo, regValue);
  if (!success) {
    return {};
  }
  buffer.resize(regInfo->byte_size);
  memcpy(buffer.data(), regValue.GetBytes(), regInfo->byte_size);

  return buffer;
}

void HostDebugger::writeRegistersData(std::span<uint8_t> data, uint64_t tid) {
  auto thread = mTarget->GetProcessSP()->GetThreadList().FindThreadByID(tid);
  if (tid == 0)
    thread = mTarget->GetProcessSP()->GetThreadList().GetThreadAtIndex(0);

  auto regContext = thread->GetRegisterContext();
  for (uint32_t regNum = 0; regNum < regContext->GetRegisterCount(); regNum++) {
    const RegisterInfo *regInfo = regContext->GetRegisterInfoAtIndex(regNum);
    if (regInfo == nullptr) {
      std::cerr << "Failed to fetch register info\n";
      std::terminate();
    }
    if (regInfo->value_regs != nullptr)
      continue; // skip registers that are contained in other registers
    llvm::ArrayRef<uint8_t> raw(data.data() + regInfo->byte_offset,
                                regInfo->byte_size);
    RegisterValue regValue(raw, eByteOrderLittle);
    regContext->WriteRegister(regInfo, regValue);
  }
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
      mTarget->GetProcessSP()->GetThreadList().FindThreadByID(threadId);

  if (threadId == 0)
    thread = mTarget->GetProcessSP()->GetThreadList().GetThreadAtIndex(0);

  auto stateType = mTarget->GetProcessSP()->GetState();

  dpcpp_trace::StopReason returnReason;

  switch (stateType) {
  case eStateExited:
  case eStateInvalid:
  case eStateUnloaded:
    returnReason.type = dpcpp_trace::StopReason::Type::exit;
    return returnReason;
  case eStateAttaching:
  case eStateLaunching:
  case eStateRunning:
  case eStateStepping:
  case eStateDetached:
    returnReason.type = dpcpp_trace::StopReason::Type::none;
    return returnReason;
  default:
    break;
  }

  auto realReason = thread->GetStopReason();
  switch (realReason) {
  case eStopReasonInvalid:
    returnReason.type = dpcpp_trace::StopReason::Type::exit;
    break;
  case eStopReasonTrace:
    returnReason.type = dpcpp_trace::StopReason::Type::trace;
    break;
  case eStopReasonNone: // Software breakpoints?
  case eStopReasonBreakpoint:
  case eStopReasonPlanComplete:
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
  default:
    returnReason.type = dpcpp_trace::StopReason::Type::none;
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
  auto process = mTarget->GetProcessSP();
  Process::StopLocker stopLocker;
  if (stopLocker.TryLock(&process->GetRunLock())) {
    std::lock_guard<std::recursive_mutex> guard(
        process->GetTarget().GetAPIMutex());

    std::vector<uint8_t> buf;
    buf.resize(len);

    Status error;
    size_t actualBytes = process->ReadMemory(addr, buf.data(), len, error);

    if (error.Fail())
      return {};

    buf.resize(actualBytes);
    return buf;
  }

  return {};
}

void HostDebugger::writeMemory(uint64_t addr, size_t len,
                               std::span<uint8_t> data) {
  auto process = mTarget->GetProcessSP();
  Process::StopLocker stopLocker;
  if (stopLocker.TryLock(&process->GetRunLock())) {
    std::lock_guard<std::recursive_mutex> guard(
        process->GetTarget().GetAPIMutex());

    Status error;
    process->WriteMemory(addr, data.data(), len, error);
  }
}

std::string HostDebugger::getExecutablePath() { return mExecutablePath; }

static llvm::StringRef getEncodingNameOrEmpty(const RegisterInfo &regInfo) {
  switch (regInfo.encoding) {
  case eEncodingUint:
    return "uint";
  case eEncodingSint:
    return "sint";
  case eEncodingIEEE754:
    return "ieee754";
  case eEncodingVector:
    return "vector";
  default:
    return "";
  }
}

static llvm::StringRef getFormatNameOrEmpty(const RegisterInfo &regInfo) {
  switch (regInfo.format) {
  case eFormatBinary:
    return "binary";
  case eFormatDecimal:
    return "decimal";
  case eFormatHex:
    return "hex";
  case eFormatFloat:
    return "float";
  case eFormatVectorOfSInt8:
    return "vector-sint8";
  case eFormatVectorOfUInt8:
    return "vector-uint8";
  case eFormatVectorOfSInt16:
    return "vector-sint16";
  case eFormatVectorOfUInt16:
    return "vector-uint16";
  case eFormatVectorOfSInt32:
    return "vector-sint32";
  case eFormatVectorOfUInt32:
    return "vector-uint32";
  case eFormatVectorOfFloat32:
    return "vector-float32";
  case eFormatVectorOfUInt64:
    return "vector-uint64";
  case eFormatVectorOfUInt128:
    return "vector-uint128";
  default:
    return "";
  };
}

// TODO find a portable way of doing the same for GDB.
std::string HostDebugger::getGDBTargetXML() {

  std::string result = "<?xml version=\"1.0\"?>\n"
                       "<target version=\"1.0\">\n";
  result +=
      fmt::format("<architecture>{}</architecture>\n",
                  mTarget->GetArchitecture().GetTriple().getArchName().str());

  result += "<feature>\n";

  auto thread = mTarget->GetProcessSP()->GetThreadList().GetThreadAtIndex(0);

  auto regContext = thread->GetRegisterContext();

  for (size_t regIndex = 0; regIndex < regContext->GetRegisterCount();
       regIndex++) {
    const RegisterInfo *regInfo = regContext->GetRegisterInfoAtIndex(regIndex);

    result += fmt::format("<reg name=\"{}\" bitsize=\"{}\" regnum=\"{}\" ",
                          regInfo->name, regInfo->byte_size * 8, regIndex);
    result += fmt::format("offset=\"{}\" ", regInfo->byte_offset);

    if (regInfo->alt_name && regInfo->alt_name[0])
      result += fmt::format("altname=\"{}\" ", regInfo->alt_name);

    llvm::StringRef encoding = getEncodingNameOrEmpty(*regInfo);
    if (!encoding.empty())
      result += fmt::format("encoding=\"{}\" ", encoding);

    llvm::StringRef format = getFormatNameOrEmpty(*regInfo);
    if (!format.empty())
      result += fmt::format("format=\"{}\" ", format);

    result += "/>\n";
  }

  result += "</feature>\n</target>";

  return result;
}

dpcpp_trace::ProcessInfo HostDebugger::getProcessInfo() {
  if (!mTarget) {
    std::cerr << "Invalid target\n";
    std::terminate();
  }
    ProcessInstanceInfo info;
    if (!mTarget->GetProcessSP()->GetProcessInfo(info)) {
      std::cerr << "Failed to get process info\n";
      std::terminate();
    }
    const ArchSpec &procArch = info.GetArchitecture();
    if (!procArch.IsValid()) {
      std::cerr << "Failed to get process info\n";
      std::terminate();
    }

    std::string endian;
    switch (procArch.GetByteOrder()) {
    case lldb::eByteOrderLittle:
      endian = "little";
      break;
    case lldb::eByteOrderBig:
      endian = "big";
      break;
    case lldb::eByteOrderPDP:
      endian = "pdp";
      break;
    default:
      // Nothing.
      break;
    }

    const llvm::Triple &triple = procArch.GetTriple();
    dpcpp_trace::ProcessInfo returnInfo{
        info.GetProcessID(), info.GetParentProcessID(), info.GetUserID(), info.GetGroupID(), info.GetEffectiveUserID(),
        info.GetEffectiveGroupID(),
        triple.getTriple(),
        triple.getOSName().str(),
        endian,
        procArch.GetTargetABI(),
        procArch.GetAddressByteSize()
    };

    return returnInfo;
}

void HostDebugger::createSoftwareBreakpoint(uint64_t address) {
  if (mTarget) {
    std::lock_guard guard(mTarget->GetAPIMutex());
    auto bp = mTarget->CreateBreakpoint(address, false, false);
    if (bp) {
      mBreakpoints[address] = bp;
    }
  }
}

void HostDebugger::removeSoftwareBreakpoint(uint64_t address) {
  if (mTarget) {
    std::lock_guard guard(mTarget->GetAPIMutex());

    if (mBreakpoints.contains(address)) {
      mTarget->RemoveBreakpointByID(mBreakpoints[address]->GetID());
      mBreakpoints.erase(address);
    }
  }
}

void HostDebugger::resume(int signal, uint64_t tid) {
  mTarget->GetProcessSP()->GetThreadList().SetSelectedThreadByID(tid);
  Status error = mTarget->GetProcessSP()->ResumeSynchronous(nullptr);
  if (error.Fail()) {
    std::cerr << "Error resuming process: " << error.AsCString() << "\n";
    std::terminate();
  }
}

void HostDebugger::stepInstruction(uint64_t tid, int signal) {
  auto thread = mTarget->GetProcessSP()->GetThreadList().FindThreadByID(tid);
  if (tid == 0)
    thread = mTarget->GetProcessSP()->GetThreadList().GetThreadAtIndex(0);

  if (!thread) {
    std::cerr << "Failed to find thread with id " << tid << "\n";
    std::terminate();
  }

  if (signal != 0) {
    thread->SetResumeSignal(signal);
  }

  ExecutionContext exe;

  thread->CalculateExecutionContext(exe);

  if (!exe.HasThreadScope()) {
    std::cerr << "Thread object is invalid\n";
    std::terminate();
  }

  Status error;

  ThreadPlanSP newPlan(thread->QueueThreadPlanForStepSingleInstruction(
      false, true, true, error));
  if (error.Fail()) {
    std::cerr << "Failed to create thread plan: " << error.AsCString() << "\n";
    std::terminate();
  }

  resumeNewPlan(exe, newPlan.get());
}

void HostDebugger::resumeNewPlan(ExecutionContext &ctx, ThreadPlan *newPlan) {
  Process *proc = ctx.GetProcessPtr();
  Thread *thread = ctx.GetThreadPtr();
  if (!proc || !thread) {
    std::cerr << "Invalid execution context\n";
    std::terminate();
  }

  if (newPlan != nullptr) {
    newPlan->SetIsMasterPlan(true);
    newPlan->SetOkayToDiscard(false);
  }

  // Why do we need to set the current thread by ID here???
  proc->GetThreadList().SetSelectedThreadByID(thread->GetID());
  proc->ResumeSynchronous(nullptr);
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
