#include "active_event_snapshot.hpp"

#include <cassert>
#include <iostream>
#include <optional>
#include <vector>

int main() {
  using aquarium::ActiveEventSnapshot;
  using aquarium::EventState;
  using aquarium::EventType;
  using aquarium::Severity;
  using aquarium::TemperatureEvent;

  ActiveEventSnapshot snapshot;
  snapshot.apply({TemperatureEvent{EventType::high_temperature,
                                   EventState::opened, Severity::n2,
                                   1000, 27.8}});
  assert(snapshot.events().size() == 1U);
  assert(snapshot.events()[0].state == EventState::opened);
  assert(snapshot.events()[0].severity == Severity::n2);
  assert(snapshot.events()[0].display_c == std::optional<double>{27.8});

  snapshot.apply({TemperatureEvent{EventType::high_temperature,
                                   EventState::escalated, Severity::n3,
                                   2000, 28.8}});
  assert(snapshot.events().size() == 1U);
  assert(snapshot.events()[0].state == EventState::escalated);
  assert(snapshot.events()[0].severity == Severity::n3);
  assert(snapshot.events()[0].at_ms == 2000U);

  snapshot.apply({TemperatureEvent{EventType::high_temperature,
                                   EventState::reminder, Severity::n3,
                                   3000, 28.9}});
  assert(snapshot.events().size() == 1U);
  assert(snapshot.events()[0].state == EventState::reminder);
  assert(snapshot.events()[0].at_ms == 3000U);

  snapshot.apply({TemperatureEvent{EventType::sensor_fault,
                                   EventState::opened, Severity::n2,
                                   4000, 0.0}});
  assert(snapshot.events().size() == 2U);
  assert(snapshot.events()[1].type == EventType::sensor_fault);
  assert(!snapshot.events()[1].display_c.has_value());

  snapshot.apply({TemperatureEvent{EventType::high_temperature,
                                   EventState::resolved, Severity::n3,
                                   5000, 26.4}});
  assert(snapshot.events().size() == 1U);
  assert(snapshot.events()[0].type == EventType::sensor_fault);

  std::cout << "active event snapshot tests passed\n";
}
