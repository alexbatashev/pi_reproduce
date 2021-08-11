add_subdirectory(graph_dump)
add_subdirectory(plugin_record)
add_subdirectory(plugin_replay)
add_subdirectory(system_intercept)
add_subdirectory(utils)

if (BUILD_DEBUGGER)
  add_subdirectory(debug)
endif()
