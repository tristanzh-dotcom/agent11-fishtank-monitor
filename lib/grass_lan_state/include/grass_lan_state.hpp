#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace aquarium::grass_lan {

constexpr std::size_t kPacketSize = 72U;

enum class ThermalState : std::uint8_t { normal, high, low, no_signal };

struct State {
  std::uint64_t sampled_at_ms{};
  std::array<std::optional<float>, 2> temperature_c{};
  std::array<ThermalState, 2> thermal_state{
      ThermalState::no_signal, ThermalState::no_signal};
  bool has_packet{};
  bool fresh{};
};

struct DecodedState {
  State snapshot{};
  std::uint64_t source_id{};
  std::uint32_t sequence{};
};

std::array<std::uint8_t, kPacketSize> encode(
    const State& snapshot, std::uint64_t source_id, std::uint32_t sequence,
    const std::array<std::uint8_t, 32>& key);

bool decode(const std::array<std::uint8_t, kPacketSize>& packet,
            const std::array<std::uint8_t, 32>& key, DecodedState* output);

struct Reducer {
  std::uint64_t source_id{};
  std::uint64_t retired_source_id{};
  std::uint32_t sequence{};
  std::uint64_t accepted_at_ms{};
  State snapshot{};
  bool has_packet{};
};

bool accept(Reducer* reducer,
            const std::array<std::uint8_t, kPacketSize>& packet,
            std::uint64_t now_ms, const std::array<std::uint8_t, 32>& key);
State freshState(const Reducer& reducer, std::uint64_t now_ms);

enum class ConnectivityEvent : std::uint8_t { none, offline, recovered };
struct ConnectivityTracker {
  bool initialized{};
  bool offline{};
};
ConnectivityEvent observeConnectivity(ConnectivityTracker* tracker,
                                      const State& state);
void begin();
void tick(std::uint64_t now_ms);
State snapshot(std::uint64_t now_ms);

}  // namespace aquarium::grass_lan
