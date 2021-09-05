#include "Recorder.hpp"

#include "pi_arguments_handler.hpp"

namespace dpcpp_trace::detail {
void fillPIArgs(APICall *call, uint32_t functionID, void *args) {
  sycl::xpti_helpers::PiArgumentsHandler argHandler;
#define _PI_API(api)                                                           \
  argHandler.set##_##api([&](const pi_plugin &, std::optional<pi_result>,      \
                             auto &&...args) { collectArgs(call, args...); });
#include <CL/sycl/detail/pi.def>
#undef _PI_API
  pi_plugin plugin;
  argHandler.handle(functionID, plugin, std::nullopt, args);
}
} // namespace dpcpp_trace::detail
