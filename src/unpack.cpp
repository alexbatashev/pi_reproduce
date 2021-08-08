#include "common.hpp"
#include "constants.hpp"
#include "utils/Compression.hpp"
#include "utils/MappedFile.hpp"
#include "utils/MemoryView.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace dpcpp_trace;

void parseV0(MappedFile &mapping, fs::path output) {
  auto mapIt = mapping.begin() + 1;

  Compression comp;

  while (mapIt != mapping.end()) {
    uint8_t kind = *mapIt;
    mapIt++;

    if (kind == 0) {
      uint64_t size = *reinterpret_cast<uint64_t *>(mapIt);
      mapIt += sizeof(size);
      std::string_view path{reinterpret_cast<char *>(mapIt), size};
      mapIt += size;

      fs::create_directory(output / path);
    } else if (kind == 1) {
      uint64_t size = *reinterpret_cast<uint64_t *>(mapIt);
      mapIt += sizeof(size);
      std::string_view path{reinterpret_cast<char *>(mapIt), size};
      mapIt += size;

      uint64_t length = *reinterpret_cast<uint64_t *>(mapIt);
      mapIt += sizeof(length);

      auto buf = comp.uncompress(MemoryView{mapIt, length});
      mapIt += length;

      std::ofstream os{output / path};
      os.write(buf.as<char>(), buf.size());
      os.close();
    }
  }
}

void unpack(const options &opts) {
  if (!std::filesystem::exists(opts.input())) {
    std::cerr << "Path does not exist: " << opts.input() << "\n";
    exit(EXIT_FAILURE);
  }

  if (fs::exists(opts.output())) {
    std::cerr << "Path exists: " << opts.output() << "\n";
    exit(EXIT_FAILURE);
  }

  fs::create_directory(opts.output());

  MappedFile mapping{opts.input()};

  uint8_t version = *mapping.begin();
  if (version == 0) {
    parseV0(mapping, opts.output());
  } else {
    std::cerr << "Unknown package verion " << version << "\n";
    exit(EXIT_FAILURE);
  }
}
