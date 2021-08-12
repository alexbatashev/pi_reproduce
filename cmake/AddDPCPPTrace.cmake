macro(set_dpcpp_trace_common_options target_name)
  if (NOT OpenCL_INCLUDE_DIRS)
    find_package(OpenCL REQUIRED)
  endif()

  target_include_directories(${target_name} PRIVATE
    ${OpenCL_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_BINARY_DIR}/include
    ${INTEL_LLVM_BINARY_PATH}/include/sycl
    ${INTEL_LLVM_SOURCE_PATH}/sycl/tools/xpti_helpers
    ${INTEL_LLVM_SOURCE_PATH}/xpti/include)
  target_link_directories(${target_name} PRIVATE ${INTEL_LLVM_BINARY_PATH}/lib)

  target_compile_definitions(${target_name} PRIVATE -DCL_TARGET_OPENCL_VERSION=300)

  set_target_properties(${target_name} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/bin
    )

  if (DPCPP_TRACE_LINK_STATICALLY)
    if (NOT WIN32)
      target_compile_options(${target_name} PRIVATE libc.a -static-libstdc++ -static-libgcc)
      target_link_options(${target_name} PRIVATE libc.a -static-libstdc++ -static-libgcc)
    endif()
  endif()
endmacro()

function(add_dpcpp_trace_library target_name kind)
  add_library(${target_name} ${kind} ${ARGN})

  set_dpcpp_trace_common_options(${target_name})

  target_compile_options(${target_name} PRIVATE -fPIC)

  set_target_properties(${target_name} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/lib
    )
endfunction()

function(add_dpcpp_trace_executable target_name)
  add_executable(${target_name} ${ARGN})

  set_dpcpp_trace_common_options(${target_name})
endfunction()
