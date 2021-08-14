#include <catch2/catch.hpp>
#include <stdexcept>

#include "HostDebugger.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

TEST_CASE("successfully attaches to process", "[HostDebugger]") {
  DebuggerInfo info;
  initialize(&info);
  HostDebugger debugger;

  std::array<std::string, 1> cargs = {"ls"};
  std::array<std::string, 1> cenv = {"A=B"};

  REQUIRE_NOTHROW(debugger.launch("/usr/bin/ls", cargs, cenv));
  REQUIRE_NOTHROW(debugger.detach());
}
