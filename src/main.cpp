#include "active_event_snapshot.hpp"
#include "bark_notifier.hpp"
#include "config.hpp"
#include "delivery_coordinator.hpp"
#include "ds18b20_reader.hpp"
#include "heartbeat_contract.hpp"
#include "heartbeat_notifier.hpp"
#include "secrets.hpp"
#include "retry_backoff.hpp"

#include <Arduino.h>
#include <WiFi.h>

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

void print_sample(const aquarium::TemperatureSample& sample) {
  Serial.print("sample at_ms=");
  Serial.print(sample.at_ms);
  Serial.print(' ');
  print_temperature("main_tank", sample.display_c);
  Serial.print(' ');
  print_temperature("sump_tank", sample.return_c);
  Serial.println();
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

  const auto sample = reader.read(now_ms);
  print_sample(sample);
  const auto events = engine.ingest(sample);
  active_events.apply(events);
  for (const auto& event : events) {
    delivery.enqueue(event);
  }

  if (const auto* event = delivery.bark_front(); event != nullptr) {
    delivery.acknowledge_bark(bark.notify(*event, runtime_config.aquarium_id));
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
