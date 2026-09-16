#include "extension_lan_state.hpp"

#include <cassert>

using aquarium::extension_lan::Reducer;
using aquarium::extension_lan::State;
using aquarium::extension_lan::ThermalState;

int main() {
  std::array<std::uint8_t, 32> key{};
  State source{};
  source.sampled_at_ms = 1000U;
  source.temperature_c = {25.4F, 27.1F};
  source.thermal_state = {ThermalState::normal, ThermalState::high};
  const auto packet = aquarium::extension_lan::encode(source, 7U, 1U, key);
  Reducer reducer{};
  assert(aquarium::extension_lan::accept(&reducer, packet, 1100U, key));
  const auto state = aquarium::extension_lan::freshState(reducer, 1100U);
  assert(state.temperature_c[0].has_value());
  assert(*state.temperature_c[0] == 25.4F);
  assert(!aquarium::extension_lan::accept(&reducer, packet, 1200U, key));
  auto tampered = packet;
  tampered[29] ^= 1U;
  assert(!aquarium::extension_lan::accept(&reducer, tampered, 1300U, key));
  assert(aquarium::extension_lan::freshState(reducer, 76101U)
             .temperature_c[0]
             .has_value() == false);
}
