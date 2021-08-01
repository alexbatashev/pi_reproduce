add_dpcpp_trace_executable(UtilsTests
  main.cpp
  utils.cpp
  info.cpp
  record.cpp
  )
target_link_libraries(UtilsTests PRIVATE Catch2::Catch2 utils)
target_include_directories(UtilsTests PRIVATE ${PROJECT_SOURCE_DIR}/src)
catch_discover_tests(UtilsTests)

