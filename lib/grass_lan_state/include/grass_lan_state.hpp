#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace aquarium::grass_lan {

constexpr std::size_t kPacketSize = 72U;

enum class ThermalState : std::uint8_t { normal, high, low, no_signal };

struct State {
  std::uint64_t sampled_at_ms{};
  // Receiver-local diagnostics, excluded from the TGR1 wire packet.
  std::uint64_t received_at_ms{};
  std::uint64_t receive_gap_ms{};
  std::uint32_t sequence{};
  std::uint32_t missed_frames{};
  bool sender_session_changed{};
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
struct ReceiveCounters {
  std::uint32_t received{};
  std::uint32_t accepted{};
  std::uint32_t rejected{};
};
ConnectivityEvent observeConnectivity(ConnectivityTracker* tracker,
                                      const State& state);
void begin();
void tick(std::uint64_t now_ms);
State snapshot(std::uint64_t now_ms);
ReceiveCounters receiveCounters();

}  // namespace aquarium::grass_lan
