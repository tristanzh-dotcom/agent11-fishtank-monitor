#pragma once

#include "temperature_engine.hpp"
#include "daily_summary.hpp"
#include "transport_contract.hpp"

namespace aquarium::firmware {

class BarkNotifier {
 public:
  bool notify(const TemperatureEvent& event, const char* aquarium_id);
  bool notify(const transport::ScopedTemperatureEvent& event,
              const char* aquarium_id);
  bool notify(const transport::DailyTemperatureSummary& summary,
              const char* aquarium_id);
};

}  // namespace aquarium::firmware
