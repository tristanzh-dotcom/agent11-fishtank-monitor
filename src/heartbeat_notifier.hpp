#pragma once

#include "active_event_snapshot.hpp"
#include "temperature_engine.hpp"

#include <cstdint>

namespace aquarium::firmware {

class HeartbeatNotifier {
 public:
  void begin_time_sync();
  bool notify(const TemperatureSample& sample,
              const ActiveEventSnapshot& active_events,
              std::uint64_t uptime_ms);
};

}  // namespace aquarium::firmware
