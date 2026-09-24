#pragma once
#include <cstddef>
#include <cstring>
// Synthetic zero keys only. No host or device configuration is read.
class Preferences {
 public:
  bool begin(const char*, bool) { return true; }
  std::size_t getBytesLength(const char*) { return 32U; }
  std::size_t getBytes(const char*, void* out, std::size_t size) {
    std::memset(out, 0, size);
    return size;
  }
  void end() {}
};
