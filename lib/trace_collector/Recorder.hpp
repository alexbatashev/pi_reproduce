#pragma once

#include "api_call.pb.h"
#include <google/protobuf/arena.h>

#include <filesystem>
#include <fstream>
#include <numeric>
#include <span>
#include <string_view>
#include <vector>

namespace dpcpp_trace {
class Recorder {
public:
  Recorder(std::filesystem::path recordPath, APICall_APIKind kind)
      : mKind(kind) {
    mStream = std::ofstream(recordPath, std::ios::binary);
  }

  void recordBegin(uint64_t universalID, uint64_t uniqueID, uint32_t functionID,
                   std::string_view callName, uint64_t timestamp, void *args);

  void recordEnd(uint64_t timestamp, void *result);

  void flush();

private:
  google::protobuf::Arena mArena;
  APICall_APIKind mKind;
  std::vector<APICall *> mCallData;
  std::ofstream mStream;
};

namespace detail {
void fillPIArgs(APICall *call, uint32_t functionID, void *args);
void fillOCLArgs(APICall *call, uint32_t functionID, void *args);

template <typename T, typename... Ts>
static void collectArgs(dpcpp_trace::APICall *call, T &&arg, Ts &&...rest) {
  auto &savedArg =
      *google::protobuf::Arena::CreateMessage<ArgData>(call->GetArena());
  call->mutable_args()->AddAllocated(&savedArg);
  if constexpr (std::is_same_v<T, const char *>) {
    savedArg.set_type(dpcpp_trace::ArgData::STRING);
    savedArg.set_str_val(arg);
  } else if constexpr (std::is_pointer_v<std::remove_reference_t<T>>) {
    uint64_t ptr = std::bit_cast<uint64_t>(arg);
    savedArg.set_type(dpcpp_trace::ArgData::POINTER);
    savedArg.set_int_val(ptr);
  } else if constexpr (std::is_unsigned_v<T>) {
    uint64_t val = static_cast<uint64_t>(arg);
    savedArg.set_int_val(val);

    if (sizeof(T) == 4)
      savedArg.set_type(dpcpp_trace::ArgData::UINT32);
    else if (sizeof(T) == 8)
      savedArg.set_type(dpcpp_trace::ArgData::UINT64);
  } else {
    int64_t val = static_cast<int64_t>(arg);
    savedArg.set_int_val(std::bit_cast<uint64_t>(val));

    if (sizeof(T) == 4)
      savedArg.set_type(dpcpp_trace::ArgData::INT32);
    else if (sizeof(T) == 8)
      savedArg.set_type(dpcpp_trace::ArgData::INT64);
  }

  if constexpr (sizeof...(Ts) > 0) {
    collectArgs(call, rest...);
  }
}
} // namespace detail
} // namespace dpcpp_trace
