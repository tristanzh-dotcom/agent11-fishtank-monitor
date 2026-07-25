#pragma once

#include "temperature_engine.hpp"

#include <array>
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

struct RuntimeConfig {
  TemperaturePolicy policy{};
  std::uint32_t sample_interval_ms = 30000U;
  const char* aquarium_id = "tank01";
  // Fixed after the 2026-07-24 desktop validation of both physical probes.
  SensorRomAddress main_tank_sensor{};
  SensorRomAddress sump_return_sensor{};
  bool bark_enabled = true;
  bool mqtt_enabled = false;
  bool heartbeat_enabled = true;
  std::uint32_t heartbeat_interval_ms = 300000U;
};

constexpr RuntimeConfig default_runtime_config() {
  RuntimeConfig config{};
  config.main_tank_sensor =
      SensorRomAddress{{0x28, 0x20, 0xD5, 0x6B, 0x11, 0x00, 0x00, 0xC1}};
  config.sump_return_sensor =
      SensorRomAddress{{0x28, 0x17, 0xAE, 0x6B, 0x11, 0x00, 0x00, 0xB4}};
  return config;
}

}  // namespace aquarium::firmware
