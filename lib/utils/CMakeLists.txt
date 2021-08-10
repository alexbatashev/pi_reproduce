add_dpcpp_trace_library(utils STATIC
  options.cpp
  utils.cpp
  trace.cpp
  Tracer.cpp
  MappedFile.cpp
  Compression.cpp
)

target_link_libraries(utils PUBLIC CONAN_PKG::zstd)
