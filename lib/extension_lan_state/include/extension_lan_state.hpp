#pragma once

#include "temperature_engine.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace aquarium::extension_lan {

constexpr std::size_t kPacketSize = 72U;
constexpr std::size_t kSlotCount = 2U;
constexpr std::size_t kReservedNoSignalSlot = 0U;
constexpr std::size_t kPlecoSlot = 1U;
constexpr std::uint64_t kFreshnessMs = 75'000U;
constexpr std::int16_t kMissingTemperature = -32768;

enum class ThermalState : std::uint8_t { normal, high, low, no_signal };

struct State {
  std::uint64_t sampled_at_ms{};
  bool fresh{};
  // TEX1 keeps slot 0 as a no-signal reservation; slot 1 carries the pleco
  // tank. The frame shape remains two slots for compatibility.
  std::array<std::optional<float>, kSlotCount> temperature_c{};
  std::array<ThermalState, kSlotCount> thermal_state{
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
std::optional<TemperatureSample> nextTemperatureSample(
    const State& state, std::uint64_t at_ms,
    std::optional<std::uint64_t>* last_source_sampled_at_ms);
void begin();
void tick(std::uint64_t now_ms);
State snapshot(std::uint64_t now_ms);

}  // namespace aquarium::extension_lan
