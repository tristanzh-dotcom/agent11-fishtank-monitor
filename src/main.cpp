#include "aliyun_mqtt.hpp"
#include "bark_notifier.hpp"
#include "config.hpp"
#include "delivery_coordinator.hpp"
#include "ds18b20_reader.hpp"
#include "secrets.hpp"
#include "retry_backoff.hpp"

#include <Arduino.h>
#include <WiFi.h>

namespace {

constexpr std::uint8_t kOneWirePin = 4;

aquarium::firmware::RuntimeConfig runtime_config =
    aquarium::firmware::default_runtime_config();
aquarium::TemperatureEngine engine(runtime_config.policy);
aquarium::firmware::Ds18b20Reader reader(kOneWirePin, runtime_config);
aquarium::firmware::AliyunMqtt mqtt;
aquarium::firmware::BarkNotifier bark;
aquarium::DeliveryCoordinator delivery(16, runtime_config.bark_enabled,
                                       runtime_config.mqtt_enabled);
aquarium::RetryBackoff wifi_backoff(1000U, 60000U);

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

void connect_wifi(std::uint64_t now_ms) {
  if (WiFi.status() == WL_CONNECTED) {
    wifi_backoff.record_success();
    return;
  }
  if (!wifi_backoff.should_attempt(now_ms)) {
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(aquarium::secrets::kWifiSsid, aquarium::secrets::kWifiPassword);
  wifi_backoff.record_failure(now_ms);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  connect_wifi(monotonic_millis());
  reader.begin();
}

void loop() {
  static std::uint64_t last_sample_at_ms = 0;
  const std::uint64_t now_ms = monotonic_millis();

  connect_wifi(now_ms);
  if (runtime_config.mqtt_enabled) {
    mqtt.loop();
  }

  if (now_ms - last_sample_at_ms < runtime_config.sample_interval_ms) {
    delay(50);
    return;
  }
  last_sample_at_ms = now_ms;

  const auto sample = reader.read(now_ms);
  if (runtime_config.mqtt_enabled) {
    mqtt.publish_telemetry(sample, now_ms);
  }
  for (const auto& event : engine.ingest(sample)) {
    delivery.enqueue(event);
  }

  if (const auto* event = delivery.bark_front(); event != nullptr) {
    delivery.acknowledge_bark(bark.notify(*event, runtime_config.aquarium_id));
  }
  if (const auto* event = delivery.mqtt_front(); event != nullptr) {
    delivery.acknowledge_mqtt(mqtt.publish_event(*event, now_ms));
  }
}
