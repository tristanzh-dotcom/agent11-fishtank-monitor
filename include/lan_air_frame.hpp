#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace aquarium::diagnostics {
struct AirFrame {
  bool downlink{};
  bool group{};
  bool associated_ap{};
  bool retry{};
  bool encrypted{};
  std::uint16_t sequence{};
};
inline bool selectAirFrame(const std::uint8_t* data, std::size_t size,
                           const std::uint8_t* sender, const std::uint8_t* ap,
                           AirFrame* out) {
  if (!data || !sender || !ap || !out || size < 24U) return false;
  // Infrastructure data/QoS data only. Exclude null data and 4-address WDS.
  if ((data[0] & 0x0fU) != 0x08U || (data[0] & 0x40U) != 0U) return false;
  const auto direction = data[1] & 3U;
  if (direction != 1U && direction != 2U) return false;
  const bool down = direction == 2U;
  if (std::memcmp(data + (down ? 16U : 10U), sender, 6U) != 0) return false;
  out->downlink = down;
  out->group = (data[down ? 4U : 16U] & 1U) != 0U;
  out->associated_ap = std::memcmp(data + (down ? 10U : 4U), ap, 6U) == 0;
  out->retry = (data[1] & 8U) != 0U;
  out->encrypted = (data[1] & 0x40U) != 0U;
  out->sequence = (static_cast<std::uint16_t>(data[23]) << 4U) | (data[22] >> 4U);
  return true;
}
}  // namespace aquarium::diagnostics
