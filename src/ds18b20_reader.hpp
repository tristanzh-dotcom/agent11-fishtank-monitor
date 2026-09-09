#pragma once

#include "config.hpp"

#include <DallasTemperature.h>
#include <OneWire.h>

#include <array>

namespace aquarium::firmware {

class Ds18b20Reader {
 public:
  Ds18b20Reader(std::uint8_t pin, const RuntimeConfig& config);

  void begin();
  struct TemperatureReadings {
    std::uint64_t at_ms;
    TemperatureSample primary;
    std::array<std::optional<double>, 3> auxiliary_c{};
    std::array<SensorBindingState, 3> auxiliary_states{
        SensorBindingState::unconfigured, SensorBindingState::unconfigured,
        SensorBindingState::unconfigured};
  };

  TemperatureReadings read(std::uint64_t at_ms);

 private:
  std::optional<double> read_sensor(const SensorRomAddress& address);
  bool to_device_address(const SensorRomAddress& source,
                         DeviceAddress destination) const;

  RuntimeConfig config_;
  OneWire one_wire_;
  DallasTemperature sensors_;
};

}  // namespace aquarium::firmware
