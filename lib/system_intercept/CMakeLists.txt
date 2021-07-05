add_library(system_intercept SHARED threads.cpp)
target_link_libraries(system_intercept PRIVATE -ldl)
install(TARGETS system_intercept DESTINATION lib)
