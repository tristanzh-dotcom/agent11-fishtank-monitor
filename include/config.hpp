#pragma once

#include "temperature_engine.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace aquarium::firmware {

struct SensorRomAddress {
  std::array<std::uint8_t, 8> bytes{};

  constexpr bool configured() const {
    for (const auto byte : bytes) {
      if (byte != 0U) {
        return true;
      }
    }
    return false;
  }
};

enum class SensorBindingState {
  valid,
  unconfigured,
  duplicate,
};

struct AuxiliaryTankConfig {
  const char* key;
  const char* label;
  SensorRomAddress sensor;
  TemperaturePolicy policy;
};

struct RuntimeConfig {
  TemperaturePolicy policy{};
  std::uint32_t sample_interval_ms = 30000U;
  const char* aquarium_id = "tank01";
  // Fixed after the 2026-07-24 desktop validation of both physical probes.
  SensorRomAddress main_tank_sensor{};
  SensorRomAddress sump_return_sensor{};
  std::array<AuxiliaryTankConfig, 3> auxiliary_tanks{};
  bool bark_enabled = true;
  bool mqtt_enabled = false;
  bool heartbeat_enabled = true;
  std::uint32_t heartbeat_interval_ms = 300000U;
};

constexpr TemperaturePolicy auxiliary_policy(double low_attention_c,
                                             double low_critical_c,
                                             double low_recovery_c) {
  TemperaturePolicy policy{};
  policy.low_attention_c = low_attention_c;
  policy.low_critical_c = low_critical_c;
  policy.low_recovery_c = low_recovery_c;
  return policy;
}

constexpr bool same_sensor_rom(const SensorRomAddress& left,
                               const SensorRomAddress& right) {
  return left.bytes == right.bytes;
}

constexpr SensorBindingState auxiliary_sensor_binding_state(
    const RuntimeConfig& config, std::size_t index) {
  if (index >= config.auxiliary_tanks.size()) {
    return SensorBindingState::unconfigured;
  }
  const auto& candidate = config.auxiliary_tanks[index].sensor;
  if (!candidate.configured()) {
    return SensorBindingState::unconfigured;
  }
  if (same_sensor_rom(candidate, config.main_tank_sensor) ||
      same_sensor_rom(candidate, config.sump_return_sensor)) {
    return SensorBindingState::duplicate;
  }
  for (std::size_t other = 0; other < config.auxiliary_tanks.size(); ++other) {
    if (other != index && config.auxiliary_tanks[other].sensor.configured() &&
        same_sensor_rom(candidate, config.auxiliary_tanks[other].sensor)) {
      return SensorBindingState::duplicate;
    }
  }
  return SensorBindingState::valid;
}

constexpr bool sensor_roms_unique(const RuntimeConfig& config) {
  const std::array<SensorRomAddress, 5> sensors{
      config.main_tank_sensor,
      config.sump_return_sensor,
      config.auxiliary_tanks[0].sensor,
      config.auxiliary_tanks[1].sensor,
      config.auxiliary_tanks[2].sensor,
  };
  for (const auto& sensor : sensors) {
    if (!sensor.configured()) {
      return false;
    }
  }
  for (std::size_t left = 0; left < sensors.size(); ++left) {
    for (std::size_t right = left + 1; right < sensors.size(); ++right) {
      if (same_sensor_rom(sensors[left], sensors[right])) {
        return false;
      }
    }
  }
  return true;
}

constexpr RuntimeConfig default_runtime_config() {
  RuntimeConfig config{};
  config.main_tank_sensor =
      SensorRomAddress{{0x28, 0x20, 0xD5, 0x6B, 0x11, 0x00, 0x00, 0xC1}};
  config.sump_return_sensor =
      SensorRomAddress{{0x28, 0x17, 0xAE, 0x6B, 0x11, 0x00, 0x00, 0xB4}};
  config.auxiliary_tanks = std::array<AuxiliaryTankConfig, 3>{
      AuxiliaryTankConfig{"laosi_tank", "老四缸", SensorRomAddress{},
                          auxiliary_policy(22.5, 20.5, 23.5)},
      AuxiliaryTankConfig{"xiaohei_tank", "小黑缸", SensorRomAddress{},
                          auxiliary_policy(23.5, 22.5, 24.5)},
      AuxiliaryTankConfig{"maomao_tank", "毛毛缸", SensorRomAddress{},
                          auxiliary_policy(23.5, 22.5, 24.5)},
  };
  return config;
}

}  // namespace aquarium::firmware
