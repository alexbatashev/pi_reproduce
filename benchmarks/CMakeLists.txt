add_dpcpp_trace_executable(MicroBenchmarks
  FileNameCollection.cpp
  main.cpp
  )

target_link_libraries(MicroBenchmarks PRIVATE
  CONAN_PKG::nlohmann_json
  CONAN_PKG::benchmark)
