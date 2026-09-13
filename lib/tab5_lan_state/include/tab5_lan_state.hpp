#pragma once

#include "active_event_snapshot.hpp"

#include <array>
#include <cstdint>

namespace aquarium::tab5 {

constexpr std::size_t kPacketSize = 60U;
constexpr std::int16_t kMissingTemperature = -32768;
enum class ThermalState : std::uint8_t { normal, high, low, no_signal };
struct LanState {
  std::int16_t main_centi_c{kMissingTemperature};
  std::int16_t sump_centi_c{kMissingTemperature};
  ThermalState thermal_state{ThermalState::no_signal};
  std::uint8_t severity{};
  bool sensor_fault{};
};
LanState make_lan_state(const TemperatureSample& sample,
                        const ActiveEventSnapshot& active);
std::array<std::uint8_t, kPacketSize> encode_packet(
    const LanState& state, std::uint64_t source_id, std::uint32_t sequence,
    const std::array<std::uint8_t, 32>& key);

}  // namespace aquarium::tab5
