add_dpcpp_trace_executable(MiscTests
  main.cpp
  utils.cpp
  )
target_link_libraries(MiscTests PRIVATE Catch2::Catch2 options)
target_include_directories(MiscTests PRIVATE ${PROJECT_SOURCE_DIR}/src)
catch_discover_tests(MiscTests)

