#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace aquarium::extension_lan {

constexpr std::size_t kPacketSize = 72U;
constexpr std::uint64_t kFreshnessMs = 75'000U;
constexpr std::int16_t kMissingTemperature = -32768;

enum class ThermalState : std::uint8_t { normal, high, low, no_signal };

struct State {
  std::uint64_t sampled_at_ms{};
  std::array<std::optional<float>, 2> temperature_c{};
  std::array<ThermalState, 2> thermal_state{
      ThermalState::no_signal, ThermalState::no_signal};
};

struct Reducer {
  bool has_packet{};
  std::uint64_t source_id{};
  std::uint32_t sequence{};
  std::uint64_t accepted_at_ms{};
  State state{};
};

std::array<std::uint8_t, kPacketSize> encode(
    const State& state, std::uint64_t source_id, std::uint32_t sequence,
    const std::array<std::uint8_t, 32>& key);
bool accept(Reducer* reducer, const std::array<std::uint8_t, kPacketSize>& packet,
            std::uint64_t now_ms, const std::array<std::uint8_t, 32>& key);
State freshState(const Reducer& reducer, std::uint64_t now_ms);
void begin();
void tick(std::uint64_t now_ms);
State snapshot(std::uint64_t now_ms);

}  // namespace aquarium::extension_lan
