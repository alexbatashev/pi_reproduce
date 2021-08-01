configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/lit.site.cfg.py.in
  ${CMAKE_CURRENT_BINARY_DIR}/lit.site.cfg.py
)

find_program(LIT_TOOL NAMES lit llvm-lit
    HINTS ${LLVM_INTEL_BINARY_DIR}/bin)

add_custom_target(check-dpcpp_trace
  COMMAND ${LIT_TOOL} ${CMAKE_CURRENT_BINARY_DIR}
  USES_TERMINAL)
