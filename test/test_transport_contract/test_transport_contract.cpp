#include "delivery_coordinator.hpp"
#include "event_outbox.hpp"
#include "retry_backoff.hpp"
#include "transport_contract.hpp"

#include <cassert>
#include <iostream>

using aquarium::EventState;
using aquarium::EventType;
using aquarium::Severity;
using aquarium::TemperatureEvent;
using aquarium::TemperatureSample;

int main() {
  const TemperatureSample sample{30000, 26.25, 25.75};
  const auto telemetry = aquarium::transport::telemetry_json(sample);
  assert(telemetry ==
         "{\"timestamp_ms\":30000,\"display_c\":26.25,\"return_c\":25.75}");

  const TemperatureSample missing_display{60000, std::nullopt, 25.75};
  const auto missing_payload = aquarium::transport::telemetry_json(missing_display);
  assert(missing_payload.find("display_c") == std::string::npos);
  assert(missing_payload.find("return_c") != std::string::npos);

  const TemperatureEvent event{EventType::high_temperature, EventState::opened,
                               Severity::n2, 900000, 27.6};
  const auto event_payload = aquarium::transport::event_json(event);
  assert(event_payload.find("\"event_type\":\"high_temperature\"") !=
         std::string::npos);
  assert(event_payload.find("\"state\":\"opened\"") != std::string::npos);
  assert(event_payload.find(
             "\"event_id\":\"high_temperature:opened:900000\"") !=
         std::string::npos);

  const TemperatureEvent escalation{EventType::temperature_rapid_change,
                                   EventState::escalated, Severity::n3,
                                   1800000, 28.0};
  const auto escalation_payload = aquarium::transport::event_json(escalation);
  assert(escalation_payload.find(
             "\"event_type\":\"temperature_rapid_change\"") !=
         std::string::npos);
  assert(escalation_payload.find("\"state\":\"escalated\"") !=
         std::string::npos);
  assert(escalation_payload.find("\"severity\":\"N3\"") !=
         std::string::npos);
  assert(escalation_payload.find(
             "\"event_id\":\"temperature_rapid_change:escalated:1800000\"") !=
         std::string::npos);

  const auto bark = aquarium::transport::bark_message(event, "tank01");
  assert(bark.title == "鱼缸温度告警");
  assert(bark.body.find("high_temperature") != std::string::npos);
  assert(bark.group == "aquarium");
  assert(bark.level == "timeSensitive");
  assert(bark.fingerprint == "aquarium:tank01:high_temperature");
  assert(bark.body.find("password") == std::string::npos);
  assert(bark.body.find("DeviceSecret") == std::string::npos);

  const auto bark_request =
      aquarium::transport::bark_request_json(bark, "unit-test-key");
  assert(bark_request ==
         "{\"device_key\":\"unit-test-key\",\"title\":\"鱼缸温度告警\","
         "\"body\":\"事件=high_temperature 状态=opened 水温=27.6C\","
         "\"group\":\"aquarium\",\"level\":\"timeSensitive\"}");

  const aquarium::transport::BarkMessage escaped{
      "title\"", "body\\line", "group", "critical", "fingerprint"};
  const auto escaped_request =
      aquarium::transport::bark_request_json(escaped, "key\\\"");
  assert(escaped_request.find("\"device_key\":\"key\\\\\\\"\"") !=
         std::string::npos);
  assert(escaped_request.find("\"title\":\"title\\\"\"") !=
         std::string::npos);
  assert(escaped_request.find("\"body\":\"body\\\\line\"") !=
         std::string::npos);

  aquarium::EventOutbox outbox(2);
  assert(outbox.push(event));
  assert(outbox.push(TemperatureEvent{EventType::low_temperature,
                                      EventState::opened, Severity::n2,
                                      930000, 23.4}));
  assert(!outbox.push(event));
  assert(outbox.dropped_count() == 1);
  assert(outbox.front()->type == EventType::high_temperature);
  outbox.pop();
  assert(outbox.front()->type == EventType::low_temperature);

  aquarium::DeliveryCoordinator delivery(2, true, true);
  assert(delivery.enqueue(event));
  assert(delivery.bark_front() != nullptr);
  assert(delivery.mqtt_front() != nullptr);

  delivery.acknowledge_mqtt(true);
  assert(delivery.mqtt_front() == nullptr);
  assert(delivery.bark_front() != nullptr);

  delivery.acknowledge_bark(false);
  assert(delivery.bark_front() != nullptr);
  delivery.acknowledge_bark(true);
  assert(delivery.bark_front() == nullptr);

  aquarium::DeliveryCoordinator bark_only(2, true, false);
  assert(bark_only.enqueue(event));
  assert(bark_only.bark_front() != nullptr);
  assert(bark_only.mqtt_front() == nullptr);

  aquarium::DeliveryCoordinator mqtt_only(2, false, true);
  assert(mqtt_only.enqueue(event));
  assert(mqtt_only.bark_front() == nullptr);
  assert(mqtt_only.mqtt_front() != nullptr);

  aquarium::RetryBackoff retry(1000, 8000);
  assert(retry.should_attempt(0));
  retry.record_failure(0);
  assert(!retry.should_attempt(999));
  assert(retry.should_attempt(1000));
  retry.record_failure(1000);
  assert(!retry.should_attempt(2999));
  assert(retry.should_attempt(3000));
  retry.record_success();
  assert(retry.should_attempt(3000));

  std::cout << "transport contract tests passed\n";
}
