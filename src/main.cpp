#include "active_event_snapshot.hpp"
#include "bark_notifier.hpp"
#include "config.hpp"
#include "delivery_coordinator.hpp"
#include "ds18b20_reader.hpp"
#include "heartbeat_contract.hpp"
#include "heartbeat_notifier.hpp"
#include "secrets.hpp"
#include "retry_backoff.hpp"
#include "scoped_event_outbox.hpp"
#include "daily_summary.hpp"

#include <Arduino.h>
#include <WiFi.h>

#include <array>
#include <ctime>

namespace {

constexpr std::uint8_t kOneWirePin = 4;

aquarium::firmware::RuntimeConfig runtime_config =
    aquarium::firmware::default_runtime_config();
aquarium::TemperatureEngine engine(runtime_config.policy);
aquarium::ActiveEventSnapshot active_events;
aquarium::firmware::Ds18b20Reader reader(kOneWirePin, runtime_config);
aquarium::firmware::BarkNotifier bark;
aquarium::firmware::HeartbeatNotifier heartbeat;
aquarium::DeliveryCoordinator delivery(16, runtime_config.bark_enabled,
                                       runtime_config.mqtt_enabled);
std::array<aquarium::TemperatureEngine, 3> auxiliary_engines{
    aquarium::TemperatureEngine(runtime_config.auxiliary_tanks[0].policy),
    aquarium::TemperatureEngine(runtime_config.auxiliary_tanks[1].policy),
    aquarium::TemperatureEngine(runtime_config.auxiliary_tanks[2].policy)};
aquarium::transport::ScopedEventOutbox auxiliary_delivery(16);
aquarium::transport::DailySummaryScheduler daily_summary;
bool auxiliary_turn = false;
aquarium::RetryBackoff wifi_backoff(1000U, 60000U);
aquarium::heartbeat::HeartbeatSchedule heartbeat_schedule(
    runtime_config.heartbeat_interval_ms, 5000U,
    runtime_config.heartbeat_interval_ms);

std::uint64_t monotonic_millis() {
  static std::uint32_t previous = 0;
  static std::uint64_t epoch = 0;
  const std::uint32_t current = millis();
  if (current < previous) {
    epoch += (1ULL << 32U);
  }
  previous = current;
  return epoch + current;
}

void print_temperature(const char* label, const std::optional<double>& value) {
  Serial.print(label);
  Serial.print('=');
  if (value.has_value()) {
    Serial.print(*value, 2);
    Serial.print("C");
  } else {
    Serial.print("invalid");
  }
}

const char* binding_state_name(
    aquarium::firmware::SensorBindingState state) {
  switch (state) {
    case aquarium::firmware::SensorBindingState::valid:
      return "valid";
    case aquarium::firmware::SensorBindingState::unconfigured:
      return "unconfigured";
    case aquarium::firmware::SensorBindingState::duplicate:
      return "duplicate";
  }
  return "unknown";
}

void print_readings(
    const aquarium::firmware::Ds18b20Reader::TemperatureReadings& readings) {
  Serial.print("sample at_ms=");
  Serial.print(readings.at_ms);
  Serial.print(' ');
  print_temperature("main_tank", readings.primary.display_c);
  Serial.print(' ');
  print_temperature("sump_tank", readings.primary.return_c);
  for (std::size_t index = 0; index < readings.auxiliary_c.size(); ++index) {
    Serial.print(' ');
    print_temperature(runtime_config.auxiliary_tanks[index].key,
                      readings.auxiliary_c[index]);
    Serial.print(" binding=");
    Serial.print(binding_state_name(readings.auxiliary_states[index]));
  }
  Serial.println();
}

aquarium::transport::LocalDateTime local_time_from_epoch(std::time_t epoch) {
  constexpr std::time_t kShanghaiOffsetSeconds = 8 * 60 * 60;
  const std::time_t local_epoch = epoch + kShanghaiOffsetSeconds;
  std::tm broken_down{};
  if (gmtime_r(&local_epoch, &broken_down) == nullptr) {
    return {};
  }
  return aquarium::transport::LocalDateTime{
      broken_down.tm_year + 1900,
      broken_down.tm_mon + 1,
      broken_down.tm_mday,
      broken_down.tm_hour,
      broken_down.tm_min,
      broken_down.tm_sec,
      true};
}

aquarium::transport::DailyTemperatureSnapshot daily_snapshot(
    const aquarium::firmware::Ds18b20Reader::TemperatureReadings& readings) {
  aquarium::transport::DailyTemperatureSnapshot snapshot{};
  const std::time_t current_time = std::time(nullptr);
  if (current_time >= 1700000000) {
    snapshot.sampled_at = local_time_from_epoch(current_time);
  }
  snapshot.main_c = readings.primary.display_c;
  snapshot.sump_c = readings.primary.return_c;
  for (std::size_t index = 0; index < readings.auxiliary_c.size(); ++index) {
    snapshot.auxiliary_c[index] = readings.auxiliary_c[index];
    switch (readings.auxiliary_states[index]) {
      case aquarium::firmware::SensorBindingState::valid:
        snapshot.auxiliary_states[index] =
            readings.auxiliary_c[index].has_value()
                ? aquarium::transport::SummaryReadingState::valid
                : aquarium::transport::SummaryReadingState::invalid;
        break;
      case aquarium::firmware::SensorBindingState::unconfigured:
        snapshot.auxiliary_states[index] =
            aquarium::transport::SummaryReadingState::unconfigured;
        break;
      case aquarium::firmware::SensorBindingState::duplicate:
        snapshot.auxiliary_states[index] =
            aquarium::transport::SummaryReadingState::configuration_error;
        break;
    }
  }
  return snapshot;
}

void connect_wifi(std::uint64_t now_ms) {
  static bool was_connected = false;
  static bool scan_completed = false;
  if (WiFi.status() == WL_CONNECTED) {
    if (!was_connected) {
      Serial.print("wifi connected ip=");
      Serial.println(WiFi.localIP());
      was_connected = true;
    }
    wifi_backoff.record_success();
    return;
  }
  if (was_connected) {
    Serial.println("wifi disconnected");
    was_connected = false;
  }
  if (!wifi_backoff.should_attempt(now_ms)) {
    return;
  }
  WiFi.mode(WIFI_STA);
  if (!scan_completed) {
    const int network_count = WiFi.scanNetworks(false, true);
    bool configured_ssid_seen = false;
    for (int index = 0; index < network_count; ++index) {
      if (WiFi.SSID(index) == aquarium::secrets::kWifiSsid) {
        configured_ssid_seen = true;
        break;
      }
    }
    Serial.print("wifi scan count=");
    Serial.print(network_count);
    Serial.print(" configured_ssid_seen=");
    Serial.println(configured_ssid_seen ? 1 : 0);
    WiFi.scanDelete();
    scan_completed = true;
  }
  Serial.print("wifi connecting status=");
  Serial.println(static_cast<int>(WiFi.status()));
  WiFi.begin(aquarium::secrets::kWifiSsid, aquarium::secrets::kWifiPassword);
  wifi_backoff.record_failure(now_ms);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  connect_wifi(monotonic_millis());
  heartbeat.begin_time_sync();
  reader.begin();
}

void loop() {
  static std::uint64_t last_sample_at_ms = 0;
  const std::uint64_t now_ms = monotonic_millis();

  connect_wifi(now_ms);

  if (now_ms - last_sample_at_ms < runtime_config.sample_interval_ms) {
    delay(50);
    return;
  }
  last_sample_at_ms = now_ms;

  const auto readings = reader.read(now_ms);
  const auto& sample = readings.primary;
  print_readings(readings);
  const auto events = engine.ingest(sample);
  active_events.apply(events);
  for (const auto& event : events) {
    delivery.enqueue(event);
  }

  for (std::size_t index = 0; index < auxiliary_engines.size(); ++index) {
    if (readings.auxiliary_states[index] !=
        aquarium::firmware::SensorBindingState::valid) {
      continue;
    }
    const auto auxiliary_events = auxiliary_engines[index].ingest(
        aquarium::TemperatureSample{now_ms, readings.auxiliary_c[index],
                                    std::nullopt});
    for (const auto& event : auxiliary_events) {
      auxiliary_delivery.push({runtime_config.auxiliary_tanks[index].key,
                               runtime_config.auxiliary_tanks[index].label,
                               event});
    }
  }

  const std::time_t current_time = std::time(nullptr);
  if (current_time >= 1700000000) {
    const auto local_time = local_time_from_epoch(current_time);
    daily_summary.observe(local_time, now_ms, daily_snapshot(readings));
  }

  bool attempted_alert = false;
  const auto* main_event = delivery.bark_front();
  const auto* auxiliary_event = auxiliary_delivery.front();
  if (main_event != nullptr || auxiliary_event != nullptr) {
    attempted_alert = true;
    const bool choose_auxiliary =
        auxiliary_event != nullptr &&
        (main_event == nullptr || auxiliary_turn);
    if (choose_auxiliary) {
      const bool delivered = bark.notify(*auxiliary_event,
                                         runtime_config.aquarium_id);
      if (delivered) {
        auxiliary_delivery.pop();
      }
      auxiliary_turn = false;
    } else {
      delivery.acknowledge_bark(
          bark.notify(*main_event, runtime_config.aquarium_id));
      auxiliary_turn = true;
    }
  }
  daily_summary.expire(now_ms);
  if (!attempted_alert && delivery.bark_front() == nullptr &&
      auxiliary_delivery.front() == nullptr && daily_summary.pending() != nullptr) {
    daily_summary.acknowledge(
        bark.notify(*daily_summary.pending(), runtime_config.aquarium_id));
  }
  if (runtime_config.heartbeat_enabled && WiFi.status() == WL_CONNECTED &&
      heartbeat_schedule.should_attempt(now_ms)) {
    const bool delivered = heartbeat.notify(sample, active_events, now_ms);
    Serial.println(delivered ? "heartbeat delivered" : "heartbeat failed");
    if (delivered) {
      heartbeat_schedule.record_success(now_ms);
    } else {
      heartbeat_schedule.record_failure(now_ms);
    }
  }
}
