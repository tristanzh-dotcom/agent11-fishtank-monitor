#pragma once

#include "active_event_snapshot.hpp"
#include "heartbeat_contract.hpp"
#include "temperature_engine.hpp"

#include <cstdint>
#include <optional>

namespace aquarium::firmware {

class HeartbeatNotifier {
 public:
  void begin_time_sync();
  bool notify(const TemperatureSample& sample,
              const ActiveEventSnapshot& active_events,
              std::uint64_t uptime_ms,
              const std::optional<heartbeat::TemperatureSnapshot>&
                  temperature_snapshot = std::nullopt);
};

}  // namespace aquarium::firmware
