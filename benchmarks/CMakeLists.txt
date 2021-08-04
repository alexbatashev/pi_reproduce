set(BENCHMARK_ENABLE_TESTING OFF)
FetchContent_Declare(
  benchmark
  GIT_REPOSITORY https://github.com/google/benchmark.git
  GIT_TAG        v1.5.5
)

FetchContent_MakeAvailable(benchmark)

add_dpcpp_trace_executable(MicroBenchmarks
  FileNameCollection.cpp
  main.cpp
  )

target_link_libraries(MicroBenchmarks PRIVATE
  nlohmann_json::nlohmann_json
  benchmark)
