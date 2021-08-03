find_program(LIT_TOOL NAMES lit llvm-lit
    HINTS ${LLVM_INTEL_BINARY_DIR}/bin)
find_program(FILECHECK_TOOL NAMES FileCheck filecheck
    HINTS ${LLVM_INTEL_BINARY_DIR}/bin)


configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
)

set(LIT_OPTS -sv)
if (DEFINED ENV{LIT_ARGS})
  set(LIT_OPTS $ENV{LIT_ARGS})
endif()

add_custom_target(check-dpcpp_trace
  COMMAND ${LIT_TOOL} ${LIT_OPTS} ${CMAKE_CURRENT_BINARY_DIR}
  USES_TERMINAL)
