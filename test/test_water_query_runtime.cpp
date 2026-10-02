#include "extension_lan_state.hpp"
#include "daily_summary.hpp"
#include "heartbeat_contract.hpp"
#include <WiFiUdp.h>
#include <cassert>
#include <ctime>
#include <iostream>

namespace {
std::uint64_t clock_ms{};
std::uint64_t monotonic_millis() { return clock_ms; }
aquarium::extension_lan::State extension_state{};
aquarium::transport::DailyTemperatureSnapshot daily_snapshot(int, std::time_t) {
  aquarium::transport::DailyTemperatureSnapshot s{};
  s.sampled_at = {2026, 10, 2, 12, 0, 0, true};
  return s;
}
std::string actual_heartbeat_query() {
  const std::time_t sampled_at = 1790913600;
  const int readings{};
  std::optional<aquarium::heartbeat::TemperatureSnapshot> temperature_snapshot;
  (void)extension_state;
  (void)monotonic_millis;
#include "esp1_water_query.inc"
  assert(temperature_snapshot.has_value());
  return temperature_snapshot->summary_text;
}
}  // namespace

int main() {
  namespace rx = aquarium::extension_lan;
  rx::begin();
  rx::State source{};
  source.water_temperature_c = 26.3F;
  source.water_state = rx::WaterDisplayState::active;
  const auto packet = rx::encode(source, 7U, 1U, {});
  WiFiUDP::queues[35112U].push_back({packet.begin(), packet.end()});
  rx::tick(1000U);
  clock_ms = 75000U;
  extension_state = rx::snapshot(clock_ms);
  assert(extension_state.fresh);
  assert(actual_heartbeat_query().find("养水缸：26.3°C（养水中）") != std::string::npos);
  // Simulate sensor/Bark work passing the deadline without another sample pass.
  clock_ms = 80000U;
  assert(extension_state.fresh);
  const auto expired_query = actual_heartbeat_query();
  assert(expired_query.find("养水缸：26.3°C（数据暂未更新）（状态未知）") != std::string::npos);
  assert(expired_query.find("（养水中）") == std::string::npos);
  std::cout << "query-time extension freshness test passed\n";
}
