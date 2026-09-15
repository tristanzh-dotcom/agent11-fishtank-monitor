#pragma once

#include "active_event_snapshot.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace aquarium::tab5 {

// Version 2 carries the three auxiliary tank readings in addition to the
// existing package-tank main/sump pair.  The legacy 60-byte packet remains
// understood by Tab5, so an older sender can still provide the package tank.
constexpr std::size_t kPacketSize = 72U;
constexpr std::int16_t kMissingTemperature = -32768;
enum class ThermalState : std::uint8_t { normal, high, low, no_signal };
struct LanState {
  std::int16_t main_centi_c{kMissingTemperature};
  std::int16_t sump_centi_c{kMissingTemperature};
  ThermalState thermal_state{ThermalState::no_signal};
  std::uint8_t severity{};
  bool sensor_fault{};
  std::array<std::int16_t, 3> auxiliary_centi_c{
      kMissingTemperature, kMissingTemperature, kMissingTemperature};
  std::array<ThermalState, 3> auxiliary_thermal_state{
      ThermalState::no_signal, ThermalState::no_signal,
      ThermalState::no_signal};
};
LanState make_lan_state(
    const TemperatureSample& sample,
    const std::array<std::optional<double>, 3>& auxiliary_c,
    const std::array<ThermalState, 3>& auxiliary_thermal_state,
    const ActiveEventSnapshot& active);
std::array<std::uint8_t, kPacketSize> encode_packet(
    const LanState& state, std::uint64_t source_id, std::uint32_t sequence,
    const std::array<std::uint8_t, 32>& key);

}  // namespace aquarium::tab5
