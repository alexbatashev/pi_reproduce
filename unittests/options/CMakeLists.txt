add_dpcpp_trace_executable(OptionsTests
  main.cpp
  info.cpp
  record.cpp
  )
target_link_libraries(OptionsTests PRIVATE Catch2::Catch2 options)
target_include_directories(OptionsTests PRIVATE ${PROJECT_SOURCE_DIR}/src)
catch_discover_tests(OptionsTests)
