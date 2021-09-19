#include "utils/MiResource.hpp"

namespace dpcpp_trace::detail {
MiResource GResource;

MiResource *getMiResource() { return &GResource; }
} // namespace dpcpp_trace::detail
