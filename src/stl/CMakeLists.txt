if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  add_custom_target(StandardModules
    COMMAND ${CMAKE_CXX_COMPILER} -std=c++20 -fmodules-ts
      -x c++-system-header algorithm
      -x c++-system-header array
      -x c++-system-header filesystem
      -x c++-system-header iostream
      -x c++-system-header fstream
      -x c++-system-header vector
  )
  add_dpcpp_trace_module(std
    ${CMAKE_CURRENT_SOURCE_DIR}/std.cpp
  )
  add_dependencies(std StandardModules)
endif()
