#include "common.hpp"
#include "constants.hpp"

#include <cstdlib>
#include <filesystem>
#include <folly/compression/Compression.h>
#include <folly/io/IOBuf.h>
#include <folly/system/MemoryMapping.h>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

using json = nlohmann::json;
namespace fs = std::filesystem;

void pack(const options &opts) {
  if (!std::filesystem::exists(opts.input())) {
    std::cerr << "Path does not exist: " << opts.input() << "\n";
    exit(EXIT_FAILURE);
  }

  if (std::filesystem::exists(opts.input() / kPackedDataPath)) {
    std::cerr << "Trace is already packed\n";
    exit(EXIT_FAILURE);
  }

  std::filesystem::create_directory(opts.input() / kPackedDataPath);

  std::ifstream recordFilesConfig{opts.input() / kFilesConfigName};
  json recordFiles;
  recordFilesConfig >> recordFiles;

  json replayMap;

  std::ifstream replayConfigIn{opts.input() / kReplayConfigName};
  json replayConfig;
  replayConfigIn >> replayConfig;
  replayConfigIn.close();

  std::filesystem::path executable{
      replayConfig[kReplayExecutable].get<std::string>()};
  std::filesystem::copy_file(executable, opts.input() / kPackedDataPath / "0");

  replayMap[executable.string()] = "0";

  size_t counter = 1;

  for (auto &element : recordFiles) {
    std::string candString = element.get<std::string>();
    if (candString.starts_with("/dev"))
      continue;
    if (candString.starts_with("/sys"))
      continue;
    if (candString.starts_with("/proc"))
      continue;

    std::filesystem::path candPath{candString};
    if (!std::filesystem::is_regular_file(candPath))
      continue;

    std::string newFileName = std::to_string(counter++);

    std::filesystem::copy_file(candPath,
                               opts.input() / kPackedDataPath / newFileName);
    replayMap[candString] = newFileName;
  }

  std::ofstream replayFilesMapConfig{opts.input() / kReplayFileMapConfigName};
  replayFilesMapConfig << replayMap.dump(2);
  replayFilesMapConfig.close();

  replayConfig[kRecordMode] = kRecordModeFull;

  std::ofstream replayConfigOut{opts.input() / kReplayConfigName};
  replayConfigOut << replayConfig.dump(4);
  replayConfigOut.close();

  if (!opts.output().empty()) {
    std::ofstream os{opts.output()};
    char version = 0;
    os.write(&version, sizeof(char));

    auto codec = folly::io::getCodec(folly::io::CodecType::ZSTD);

    for (auto &p : fs::recursive_directory_iterator(opts.input())) {
      auto rel = fs::relative(p, opts.input());
      std::string pathStr = rel.string();
      if (fs::is_directory(rel)) {
        char kind = 0;
        os.write(&kind, sizeof(char));
        os.write(pathStr.data(), pathStr.size());
      } else if (fs::is_regular_file(rel)) {
        char kind = 1;
        os.write(&kind, sizeof(char));
        os.write(pathStr.data(), pathStr.size());

        folly::MemoryMapping mapping{pathStr.c_str()};
        auto outBuf = folly::IOBuf::wrapBufferAsValue(mapping.range());
        auto compressedBuf = codec->compress(&outBuf);

        uint64_t length = compressedBuf->length();
        os.write(reinterpret_cast<char *>(&length), sizeof(uint64_t));
        os.write(reinterpret_cast<const char *>(compressedBuf->data()), length);
      }
    }
  }
}
