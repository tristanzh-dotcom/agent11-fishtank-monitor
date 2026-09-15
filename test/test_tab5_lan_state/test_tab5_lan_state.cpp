#include "active_event_snapshot.hpp"
#include "tab5_lan_state.hpp"

#include <array>
#include <cassert>
#include <iostream>

namespace {

constexpr std::array<std::uint8_t, 32> kTestKey{
    0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
    0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
    0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
    0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};

void test_encoder_uses_active_thermal_result_without_thresholds() {
  aquarium::ActiveEventSnapshot active;
  active.apply({aquarium::TemperatureEvent{
      aquarium::EventType::high_temperature_critical,
      aquarium::EventState::opened, aquarium::Severity::n3, 900000U, 28.6}});
  const aquarium::TemperatureSample sample{900000U, 28.63, 25.12};

  const std::array<std::optional<double>, 3> auxiliary{22.1, 28.7,
                                                        std::nullopt};
  const std::array<aquarium::tab5::ThermalState, 3> auxiliary_states{
      aquarium::tab5::ThermalState::low,
      aquarium::tab5::ThermalState::high,
      aquarium::tab5::ThermalState::no_signal};
  const auto state = aquarium::tab5::make_lan_state(
      sample, auxiliary, auxiliary_states, active);
  assert(state.thermal_state == aquarium::tab5::ThermalState::high);
  assert(state.severity == 3U);
  assert(!state.sensor_fault);
  assert(state.main_centi_c == 2863);
  assert(state.sump_centi_c == 2512);
  assert(state.auxiliary_centi_c[0] == 2210);
  assert(state.auxiliary_centi_c[1] == 2870);
  assert(state.auxiliary_centi_c[2] == aquarium::tab5::kMissingTemperature);
  assert(state.auxiliary_thermal_state[0] ==
         aquarium::tab5::ThermalState::low);
  assert(state.auxiliary_thermal_state[1] ==
         aquarium::tab5::ThermalState::high);
  assert(state.auxiliary_thermal_state[2] ==
         aquarium::tab5::ThermalState::no_signal);

  const auto packet = aquarium::tab5::encode_packet(state, 0x1020304050607080ULL,
                                                      7U, kTestKey);
  assert(packet.size() == aquarium::tab5::kPacketSize);
  assert(packet[0] == 'T' && packet[1] == '5' && packet[4] == 2U);
  assert(packet[28] == 0x08U && packet[29] == 0xA2U);
  assert(packet[30] == 0x0BU && packet[31] == 0x36U);
  assert(packet[34] ==
         static_cast<std::uint8_t>(aquarium::tab5::ThermalState::low));
}

void test_encoder_marks_missing_or_fault_as_no_signal() {
  aquarium::ActiveEventSnapshot active;
  active.apply({aquarium::TemperatureEvent{aquarium::EventType::sensor_fault,
                                            aquarium::EventState::opened,
                                            aquarium::Severity::n2, 60000U, 0.0}});
  const auto state = aquarium::tab5::make_lan_state(
      aquarium::TemperatureSample{60000U, std::nullopt, 24.95},
      {}, {}, active);
  assert(state.thermal_state == aquarium::tab5::ThermalState::no_signal);
  assert(state.sensor_fault);
  assert(state.main_centi_c == aquarium::tab5::kMissingTemperature);
  assert(state.sump_centi_c == 2495);
}

void test_encoder_marks_valid_primary_without_events_as_normal() {
  aquarium::ActiveEventSnapshot active;
  const auto state = aquarium::tab5::make_lan_state(
      aquarium::TemperatureSample{60000U, 26.4, 25.0}, {}, {}, active);
  assert(state.thermal_state == aquarium::tab5::ThermalState::normal);
  assert(state.severity == 0U);
  assert(!state.sensor_fault);
}

}  // namespace

int main() {
  test_encoder_uses_active_thermal_result_without_thresholds();
  test_encoder_marks_missing_or_fault_as_no_signal();
  test_encoder_marks_valid_primary_without_events_as_normal();
  std::cout << "tab5 LAN state encoder tests passed\n";
  return 0;
}
