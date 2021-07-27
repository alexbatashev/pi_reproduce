#pragma once

#include <CL/sycl/detail/pi.h>

#include <fstream>
#include <istream>
#include <string>
#include <vector>

struct __attribute__((packed)) RecordHeader {
  uint32_t functionId;
  uint64_t start;
  uint64_t end;
  size_t numInputs;
  size_t numOutputs;
  size_t totalSize;
};

struct MemObject {
  std::vector<char> data;
};

struct Record {
  uint32_t functionId;
  uint64_t start;
  uint64_t end;
  std::vector<char> argsData;
  std::vector<MemObject> outputs;
  pi_result result;

  std::string threadId;
};

RecordHeader readHeader(std::ifstream &is);
Record getNextRecord(std::ifstream &is, std::string threadId,
                     bool readMemObjects = false);
