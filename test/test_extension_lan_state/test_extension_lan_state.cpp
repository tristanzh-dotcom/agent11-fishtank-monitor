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
  assert(!state.temperature_c[0].has_value());
  assert(state.thermal_state[0] == ThermalState::no_signal);
  assert(state.temperature_c[1].has_value());
  assert(*state.temperature_c[1] == 27.1F);
  assert(!aquarium::extension_lan::accept(&reducer, packet, 1200U, key));
  auto tampered = packet;
  tampered[29] ^= 1U;
  assert(!aquarium::extension_lan::accept(&reducer, tampered, 1300U, key));
  State inconsistent = source;
  inconsistent.thermal_state[0] = ThermalState::no_signal;
  const auto inconsistent_packet =
      aquarium::extension_lan::encode(inconsistent, 7U, 2U, key);
  assert(!aquarium::extension_lan::accept(&reducer, inconsistent_packet, 1400U,
                                          key));
  assert(aquarium::extension_lan::freshState(reducer, 76101U)
             .temperature_c[0]
             .has_value() == false);

  std::optional<std::uint64_t> last_source_sampled_at_ms;
  const auto fresh = aquarium::extension_lan::freshState(reducer, 1200U);
  const auto first_sample = aquarium::extension_lan::nextTemperatureSample(
      fresh, 2000U, &last_source_sampled_at_ms);
  assert(first_sample.has_value());
  assert(first_sample->display_c.has_value());
  assert(static_cast<float>(*first_sample->display_c) == 27.1F);
  assert(first_sample->at_ms == 2000U);

  const auto duplicate_sample =
      aquarium::extension_lan::nextTemperatureSample(
          fresh, 5000U, &last_source_sampled_at_ms);
  assert(!duplicate_sample.has_value());

  const auto stale = aquarium::extension_lan::freshState(reducer, 76101U);
  const auto invalid_sample = aquarium::extension_lan::nextTemperatureSample(
      stale, 80000U, &last_source_sampled_at_ms);
  assert(invalid_sample.has_value());
  assert(!invalid_sample->display_c.has_value());
}
