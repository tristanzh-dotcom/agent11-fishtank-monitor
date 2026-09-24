#include "grass_lan_state.hpp"

#include <cassert>

int main() {
  using namespace aquarium::grass_lan;
  std::array<std::uint8_t, 32> key{};
  State sample{};
  sample.sampled_at_ms = 1000U;
  sample.temperature_c[0] = 25.25F;
  sample.thermal_state[0] = ThermalState::normal;
  Reducer receiver{};
  const auto first = encode(sample, 9U, 1U, key);
  assert(accept(&receiver, first, 2000U, key));
  assert(freshState(receiver, 2000U).temperature_c[0] == 25.25F);
  assert(!accept(&receiver, first, 3000U, key));
  assert(!freshState(receiver, 77001U).fresh);
  sample.sampled_at_ms = 2000U;
  assert(accept(&receiver, encode(sample, 10U, 1U, key), 80000U, key));
  assert(!accept(&receiver, encode(sample, 9U, 2U, key), 81000U, key));
}
