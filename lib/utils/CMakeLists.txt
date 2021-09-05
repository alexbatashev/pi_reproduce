add_dpcpp_trace_library(utils STATIC
  options.cpp
  utils.cpp
  Tracer.cpp
  MappedFile.cpp
  Compression.cpp
  tls_posix.cpp
)

target_link_libraries(utils PUBLIC CONAN_PKG::zstd)
