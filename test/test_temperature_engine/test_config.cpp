#include "config.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main() {
  auto config = aquarium::firmware::default_runtime_config();
  assert(config.policy.high_attention_c == 27.5);
  assert(config.policy.high_critical_c == 28.5);
  assert(config.policy.low_attention_c == 23.5);
  assert(config.policy.low_critical_c == 22.5);
  assert(config.sample_interval_ms == 30000U);
  assert(std::string(config.aquarium_id) == "tank01");
  assert(config.bark_enabled);
  assert(!config.mqtt_enabled);
  assert(config.heartbeat_enabled);
  assert(config.heartbeat_interval_ms == 300000U);
  assert(config.main_tank_sensor.configured());
  assert(config.sump_return_sensor.configured());
  assert((config.main_tank_sensor.bytes == std::array<std::uint8_t, 8>{
      0x28, 0x20, 0xD5, 0x6B, 0x11, 0x00, 0x00, 0xC1}));
  assert((config.sump_return_sensor.bytes == std::array<std::uint8_t, 8>{
      0x28, 0x17, 0xAE, 0x6B, 0x11, 0x00, 0x00, 0xB4}));
  assert(config.main_tank_sensor.bytes != config.sump_return_sensor.bytes);
  std::cout << "firmware configuration tests passed\n";
}
