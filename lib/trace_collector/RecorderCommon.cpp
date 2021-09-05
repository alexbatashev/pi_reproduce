#include "Recorder.hpp"
#include <cstdint>

namespace dpcpp_trace {
void Recorder::recordBegin(uint64_t universalID, uint64_t uniqueID,
                           uint32_t functionID, std::string_view callName,
                           uint64_t timestamp, void *args) {
  auto *message = google::protobuf::Arena::CreateMessage<APICall>(&mArena);
  message->set_universal_id(universalID);
  message->set_unique_id(uniqueID);
  message->set_function_id(functionID);
  message->set_time_start(timestamp);
  message->set_call_name(std::string{callName});
  message->set_kind(mKind);

  mCallData.push_back(message);

  switch (mKind) {
  case APICall::PI:
    detail::fillPIArgs(message, functionID, args);
    break;
  case APICall::OPENCL:
    detail::fillOCLArgs(message, functionID, args);
  default:
    break;
  }
}

void Recorder::recordEnd(uint64_t timestamp, void *result) {
  mCallData.back()->set_time_end(timestamp);

  if (mCallData.size() > 10) {
    flush();
  }
}

void Recorder::flush() {
  for (auto *msg : mCallData) {
    std::string out;
    msg->SerializeToString(&out);
    uint64_t size = out.size();
    mStream.write(reinterpret_cast<char*>(&size), sizeof(uint64_t));
    mStream.write(out.data(), size);
  }
  mStream.flush();
  mCallData.clear();
  mArena.Reset();
}
} // namespace dpcpp_trace
