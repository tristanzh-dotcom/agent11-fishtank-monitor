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
  assert(config.policy.low_recovery_c == 23.5);
  assert(config.policy.gradient_recovery_c == 0.8);
  assert(config.sample_interval_ms == 30000U);
  assert(std::string(config.aquarium_id) == "esp1");
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

  assert(config.auxiliary_tanks.size() == 3U);
  assert(std::string(config.auxiliary_tanks[0].key) == "laosi_tank");
  assert(std::string(config.auxiliary_tanks[0].label) == "老四缸");
  assert(std::string(config.auxiliary_tanks[1].key) == "xiaohei_tank");
  assert(std::string(config.auxiliary_tanks[1].label) == "小黑缸");
  assert(std::string(config.auxiliary_tanks[2].key) == "maomao_tank");
  assert(std::string(config.auxiliary_tanks[2].label) == "毛毛缸");
  assert(config.auxiliary_tanks[0].policy.low_attention_c == 22.5);
  assert(config.auxiliary_tanks[0].policy.low_critical_c == 20.5);
  assert(config.auxiliary_tanks[0].policy.low_recovery_c == 23.5);
  assert(config.auxiliary_tanks[1].policy.low_attention_c == 23.5);
  assert(config.auxiliary_tanks[1].policy.low_critical_c == 22.5);
  assert(config.auxiliary_tanks[1].policy.low_recovery_c == 23.5);
  assert(config.auxiliary_tanks[2].policy.low_attention_c == 23.5);
  assert(config.auxiliary_tanks[2].policy.low_critical_c == 22.5);
  assert(config.auxiliary_tanks[2].policy.low_recovery_c == 23.5);
  assert(config.auxiliary_tanks[0].policy.high_attention_c == 27.5);
  assert(config.auxiliary_tanks[0].policy.high_critical_c == 28.5);
  assert(config.policy.high_recovery_c == 27.5);
  assert(config.auxiliary_tanks[0].policy.high_recovery_c == 27.5);
  assert((config.auxiliary_tanks[0].sensor.bytes ==
          std::array<std::uint8_t, 8>{0x28, 0xE9, 0x53, 0xA0, 0x11, 0x00,
                                      0x00, 0x32}));
  assert((config.auxiliary_tanks[1].sensor.bytes ==
          std::array<std::uint8_t, 8>{0x28, 0x01, 0xB7, 0x9F, 0x11, 0x00,
                                      0x00, 0xAD}));
  assert((config.auxiliary_tanks[2].sensor.bytes ==
          std::array<std::uint8_t, 8>{0x28, 0xA8, 0xE7, 0x9E, 0x11, 0x00,
                                      0x00, 0x7D}));
  assert(aquarium::firmware::sensor_roms_unique(config));

  auto unconfigured = config;
  unconfigured.auxiliary_tanks[0].sensor = {};
  assert(aquarium::firmware::auxiliary_sensor_binding_state(unconfigured, 0) ==
         aquarium::firmware::SensorBindingState::unconfigured);
  assert(!aquarium::firmware::sensor_roms_unique(unconfigured));

  auto configured = config;
  configured.auxiliary_tanks[0].sensor =
      aquarium::firmware::SensorRomAddress{{0x28, 0x01, 0x02, 0x03,
                                             0x04, 0x05, 0x06, 0x07}};
  configured.auxiliary_tanks[1].sensor =
      aquarium::firmware::SensorRomAddress{{0x28, 0x11, 0x12, 0x13,
                                             0x14, 0x15, 0x16, 0x17}};
  configured.auxiliary_tanks[2].sensor =
      aquarium::firmware::SensorRomAddress{{0x28, 0x21, 0x22, 0x23,
                                             0x24, 0x25, 0x26, 0x27}};
  assert(aquarium::firmware::sensor_roms_unique(configured));
  assert(aquarium::firmware::auxiliary_sensor_binding_state(configured, 0) ==
         aquarium::firmware::SensorBindingState::valid);
  configured.auxiliary_tanks[1].sensor = configured.main_tank_sensor;
  assert(aquarium::firmware::auxiliary_sensor_binding_state(configured, 1) ==
         aquarium::firmware::SensorBindingState::duplicate);

  std::cout << "firmware configuration tests passed\n";
}
