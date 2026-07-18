#pragma once

#include <cstdint>

namespace aquarium::firmware {

class HeartbeatNotifier {
 public:
  void begin_time_sync();
  bool notify(double main_c, double sump_c, std::uint64_t uptime_ms);
};

}  // namespace aquarium::firmware

