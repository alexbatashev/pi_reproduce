add_dpcpp_trace_executable(MicroBenchmarks
  FileNameCollection.cpp
  GraphEventsCollection.cpp
  main.cpp
  )

target_link_libraries(MicroBenchmarks PRIVATE
  trace_proto
  CONAN_PKG::nlohmann_json
  CONAN_PKG::benchmark
  CONAN_PKG::mimalloc
  utils
  )
