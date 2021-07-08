cmake_minimum_required(VERSION 3.14)

project(pi_reproduce)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(src)
add_subdirectory(lib)

enable_testing()
add_subdirectory(unittests)
