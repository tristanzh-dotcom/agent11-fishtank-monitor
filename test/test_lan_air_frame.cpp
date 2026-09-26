#include "lan_air_frame.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
  // Synthetic locally administered addresses; no registered device identities.
  const std::uint8_t sender[6]{2, 0, 0, 0, 0, 1};
  const std::uint8_t ap[6]{2, 0, 0, 0, 0, 2};
  std::array<std::uint8_t, 32> packet{};
  packet[0] = 0x08; packet[1] = 0x4a;  // data, FromDS, retry, protected
  std::memset(packet.data()+4, 255, 6);
  std::memcpy(packet.data()+10, ap, 6);
  std::memcpy(packet.data()+16, sender, 6);
  packet[22] = 0x30; packet[23] = 0x12;
  aquarium::diagnostics::AirFrame frame{};
  using aquarium::diagnostics::selectAirFrame;
  assert(selectAirFrame(packet.data(), packet.size(), sender, ap, &frame));
  assert(frame.downlink && frame.group && frame.associated_ap);
  assert(frame.retry && frame.encrypted && frame.sequence == 0x123);
  for (std::size_t size = 0; size < 24; ++size)
    assert(!selectAirFrame(packet.data(), size, sender, ap, &frame));
  packet[16] = 4;  // unrelated station must never be retained
  assert(!selectAirFrame(packet.data(), packet.size(), sender, ap, &frame));
  packet[16] = 2;
  packet[1] = 3;  // four-address/WDS layout is deliberately unsupported
  assert(!selectAirFrame(packet.data(), packet.size(), sender, ap, &frame));
  packet[1] = 2; packet[0] = 0x48;  // null data isn't a payload frame
  assert(!selectAirFrame(packet.data(), packet.size(), sender, ap, &frame));
  packet[0] = 0x88; packet[1] = 1;  // QoS uplink
  std::memcpy(packet.data()+4, ap, 6);
  std::memcpy(packet.data()+10, sender, 6);
  std::memset(packet.data()+16, 255, 6);
  assert(selectAirFrame(packet.data(), packet.size(), sender, ap, &frame));
  assert(!frame.downlink && frame.group && frame.associated_ap);
  packet[4] = 4;
  assert(selectAirFrame(packet.data(), packet.size(), sender, ap, &frame));
  assert(!frame.associated_ap);
  std::puts("PASS air_frame_target_filter_and_bounds");
}
