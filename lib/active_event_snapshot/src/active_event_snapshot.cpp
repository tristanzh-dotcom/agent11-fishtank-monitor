#include "active_event_snapshot.hpp"

#include <algorithm>

namespace aquarium {
namespace {

bool same_type(const ActiveTemperatureEvent& active, EventType type) {
  return active.type == type;
}

}  // namespace

void ActiveEventSnapshot::apply(const std::vector<TemperatureEvent>& events) {
  for (const auto& event : events) {
    const auto existing = std::find_if(
        events_.begin(), events_.end(),
        [&event](const ActiveTemperatureEvent& active) {
          return same_type(active, event.type);
        });

    if (event.state == EventState::resolved) {
      if (existing != events_.end()) {
        events_.erase(existing);
      }
      continue;
    }

    const std::optional<double> display_c =
        event.type == EventType::sensor_fault
            ? std::nullopt
            : std::optional<double>{event.display_c};
    const ActiveTemperatureEvent next{event.type, event.state, event.severity,
                                      event.at_ms, display_c};
    if (existing == events_.end()) {
      events_.push_back(next);
    } else {
      *existing = next;
    }
  }

  std::sort(events_.begin(), events_.end(),
            [](const ActiveTemperatureEvent& left,
               const ActiveTemperatureEvent& right) {
              return static_cast<int>(left.type) < static_cast<int>(right.type);
            });
}

}  // namespace aquarium
