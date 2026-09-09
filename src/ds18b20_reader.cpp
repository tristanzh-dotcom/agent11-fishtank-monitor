#include "ds18b20_reader.hpp"

#include <Arduino.h>

namespace aquarium::firmware {
namespace {

constexpr double kMinPlausibleC = 0.0;
constexpr double kMaxPlausibleC = 40.0;

}  // namespace

Ds18b20Reader::Ds18b20Reader(std::uint8_t pin, const RuntimeConfig& config)
    : config_(config), one_wire_(pin), sensors_(&one_wire_) {}

void Ds18b20Reader::begin() {
  sensors_.begin();
  sensors_.setResolution(12);
  Serial.printf("DS18B20 devices found: %u\n", sensors_.getDeviceCount());
  for (std::uint8_t index = 0; index < sensors_.getDeviceCount(); ++index) {
    DeviceAddress address{};
    if (!sensors_.getAddress(address, index)) {
      continue;
    }
    Serial.printf("DS18B20 index %u ROM:", index);
    for (const auto byte : address) {
      Serial.printf(" %02X", byte);
    }
    Serial.println();
  }
}

Ds18b20Reader::TemperatureReadings Ds18b20Reader::read(
    std::uint64_t at_ms) {
  sensors_.requestTemperatures();
  TemperatureReadings readings{at_ms,
                                TemperatureSample{
                                    at_ms, read_sensor(config_.main_tank_sensor),
                                    read_sensor(config_.sump_return_sensor)}};
  for (std::size_t index = 0; index < config_.auxiliary_tanks.size(); ++index) {
    readings.auxiliary_states[index] =
        auxiliary_sensor_binding_state(config_, index);
    if (readings.auxiliary_states[index] == SensorBindingState::valid) {
      readings.auxiliary_c[index] =
          read_sensor(config_.auxiliary_tanks[index].sensor);
    }
  }
  return readings;
}

std::optional<double> Ds18b20Reader::read_sensor(
    const SensorRomAddress& address) {
  DeviceAddress device_address{};
  if (!to_device_address(address, device_address)) {
    return std::nullopt;
  }

  const double value = sensors_.getTempC(device_address);
  if (value == DEVICE_DISCONNECTED_C || value < kMinPlausibleC ||
      value > kMaxPlausibleC) {
    return std::nullopt;
  }
  return value;
}

bool Ds18b20Reader::to_device_address(const SensorRomAddress& source,
                                      DeviceAddress destination) const {
  if (!source.configured()) {
    return false;
  }
  for (std::size_t index = 0; index < source.bytes.size(); ++index) {
    destination[index] = source.bytes[index];
  }
  return true;
}

}  // namespace aquarium::firmware
