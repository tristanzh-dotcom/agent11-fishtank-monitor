#pragma once

#include "config.hpp"

#include <DallasTemperature.h>
#include <OneWire.h>

namespace aquarium::firmware {

class Ds18b20Reader {
 public:
  Ds18b20Reader(std::uint8_t pin, const RuntimeConfig& config);

  void begin();
  TemperatureSample read(std::uint64_t at_ms);

 private:
  std::optional<double> read_sensor(const SensorRomAddress& address,
                                    std::uint8_t discovery_index);
  bool to_device_address(const SensorRomAddress& source,
                         DeviceAddress destination) const;

  RuntimeConfig config_;
  OneWire one_wire_;
  DallasTemperature sensors_;
};

}  // namespace aquarium::firmware
