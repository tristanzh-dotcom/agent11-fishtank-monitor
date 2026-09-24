#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <vector>

// Model Arduino NetworkUDP's unread-datagram behavior: parsePacket returns
// zero until read() drains the current datagram (NetworkUdp.cpp:297).
class WiFiUDP {
 public:
  inline static std::map<unsigned, std::deque<std::vector<std::uint8_t>>> queues;
  int begin(unsigned port) { port_ = port; return 1; }
  int parsePacket() {
    if (available() || queues[port_].empty()) return 0;
    packet_ = queues[port_].front();
    queues[port_].pop_front();
    cursor_ = 0;
    return static_cast<int>(packet_.size());
  }
  int available() const { return static_cast<int>(packet_.size() - cursor_); }
  int read() { return available() ? packet_[cursor_++] : -1; }
  int read(std::uint8_t* out, std::size_t size) {
    const auto count = std::min(size, static_cast<std::size_t>(available()));
    std::copy_n(packet_.data() + cursor_, count, out);
    cursor_ += count;
    return static_cast<int>(count);
  }
 private:
  unsigned port_{};
  std::vector<std::uint8_t> packet_;
  std::size_t cursor_{};
};
