#pragma once

#include "temperature_engine.hpp"

namespace aquarium::firmware {

class BarkNotifier {
 public:
  bool notify(const TemperatureEvent& event, const char* aquarium_id);
};

}  // namespace aquarium::firmware
