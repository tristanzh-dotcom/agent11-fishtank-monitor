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
  // Replace zero addresses with each probe's printed ROM before production.
  SensorRomAddress main_tank_sensor{};
  SensorRomAddress sump_return_sensor{};
  bool bark_enabled = true;
  bool mqtt_enabled = false;
  bool heartbeat_enabled = true;
  std::uint32_t heartbeat_interval_ms = 300000U;
};

constexpr RuntimeConfig default_runtime_config() {
  return RuntimeConfig{};
}

}  // namespace aquarium::firmware
