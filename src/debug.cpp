#include "common.hpp"
#include "options.hpp"

#include "debug_server/server.hpp"

#include <iostream>

void debug(const options &opts) {
  DebugServer server;
  std::cout << "Starting server on localhost:1111\n";
  server.run();
}
