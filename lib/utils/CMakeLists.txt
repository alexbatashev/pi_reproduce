find_library(ZSTD_LIB libzstd.a REQUIRED)
find_path(ZSTD_INCLUDE zstd.h REQUIRED)

add_dpcpp_trace_library(utils STATIC
  options.cpp
  utils.cpp
  trace.cpp
  MappedFile.cpp
  Compression.cpp
)

target_include_directories(utils PRIVATE ${ZSTD_INCLUDE})
target_link_libraries(utils PUBLIC ${ZSTD_LIB})
