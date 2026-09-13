#pragma once

#include "temperature_engine.hpp"
#include "daily_summary.hpp"
#include "transport_contract.hpp"

#include <ctime>
#include <optional>

namespace aquarium::firmware {

class BarkNotifier {
 public:
  bool notify(const TemperatureEvent& event, const char* aquarium_id,
              std::optional<std::time_t> event_time = std::nullopt);
  bool notify(const transport::ScopedTemperatureEvent& event,
              const char* aquarium_id,
              std::optional<std::time_t> event_time = std::nullopt);
  bool notify(const transport::DailyTemperatureSummary& summary,
              const char* aquarium_id);
};

}  // namespace aquarium::firmware
