#pragma once

#include "temperature_engine.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace aquarium {

struct ActiveTemperatureEvent {
  EventType type;
  EventState state;
  Severity severity;
  std::uint64_t at_ms;
  std::optional<double> display_c;
};

class ActiveEventSnapshot {
 public:
  void apply(const std::vector<TemperatureEvent>& events);
  const std::vector<ActiveTemperatureEvent>& events() const { return events_; }

 private:
  std::vector<ActiveTemperatureEvent> events_;
};

}  // namespace aquarium
