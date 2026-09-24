#include "delivery_coordinator.hpp"
#include "event_outbox.hpp"
#include "retry_backoff.hpp"
#include "scoped_event_outbox.hpp"
#include "transport_contract.hpp"

#include <cassert>
#include <ctime>
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

  const auto bark = aquarium::transport::bark_message(event, "esp1");
  assert(bark.title == "【注意·首次】包包缸·主缸｜高温");
  assert(bark.body.find("设备：设备") != std::string::npos);
  assert(bark.body.find("情况：当时水温：27.6°C") != std::string::npos);
  assert(bark.body.find("建议：核查最新水温及加热棒温控。") !=
         std::string::npos);
  assert(bark.body.find("时间：告警判定：时间未同步") != std::string::npos);
  assert(bark.body.find("提醒次数：本问题第 1 次告警") != std::string::npos);
  assert(bark.group == "aquarium");
  assert(bark.level == "timeSensitive");
  assert(bark.fingerprint == "aquarium:esp1:high_temperature");
  assert(bark.body.find("password") == std::string::npos);
  assert(bark.body.find("DeviceSecret") == std::string::npos);

  const auto timed_bark = aquarium::transport::bark_message(
      event, "esp1", std::time_t{1'750'000'000},
      std::time_t{1'750'000'002}, "设备");
  assert(timed_bark.body.find("时间：告警判定：2025-06-15 23:06:40") !=
         std::string::npos);
  assert(timed_bark.body.find("发送：2025-06-15 23:06:42") !=
         std::string::npos);

  const auto adjusted_clock_bark = aquarium::transport::bark_message(
      event, "esp1", std::time_t{1'750'000'002},
      std::time_t{1'750'000'000}, "设备");
  assert(adjusted_clock_bark.body.find(
             "设备时钟已调整，不能用上述时间差判断延迟。") !=
         std::string::npos);

  const auto invalid_send_time_bark = aquarium::transport::bark_message(
      event, "esp1", std::time_t{1'750'000'000}, std::time_t{-1}, "设备");
  assert(invalid_send_time_bark.body.find("发送：时间不可用") !=
         std::string::npos);

  const auto extension_offline =
      aquarium::transport::extension_connectivity_message(
          aquarium::transport::ExtensionConnectivityEvent::offline,
          75000U, std::time_t{1'750'000'000}, std::time_t{1'750'000'002});
  assert(extension_offline.title ==
         "【注意·首次】南美异形缸｜温控ESP2号连接中断");
  assert(extension_offline.body.find(
             "情况：已连续超过 75 秒未收到设备状态更新。") !=
         std::string::npos);
  assert(extension_offline.body.find("设备：温控ESP2号") !=
         std::string::npos);
  assert(extension_offline.body.find("提醒次数：本次中断仅提醒一次") !=
         std::string::npos);
  assert(extension_offline.level == "timeSensitive");

  const auto extension_recovered =
      aquarium::transport::extension_connectivity_message(
          aquarium::transport::ExtensionConnectivityEvent::recovered,
          75000U, std::time_t{1'750'000'000}, std::time_t{1'750'000'002});
  assert(extension_recovered.title ==
         "【信息·恢复】南美异形缸｜温控ESP2号连接已恢复");
  assert(extension_recovered.body.find("情况：已重新收到设备状态更新。") !=
         std::string::npos);
  assert(extension_recovered.body.find("提醒次数：不计入告警次数") !=
         std::string::npos);
  assert(extension_recovered.level == "active");

  const auto grass_offline = aquarium::transport::extension_connectivity_message(
      aquarium::transport::ExtensionConnectivityEvent::offline,
      75000U, std::time_t{1'750'000'000}, std::time_t{1'750'000'002},
      "南美草缸", "ESP3", "esp3");
  assert(grass_offline.title == "【注意·首次】南美草缸｜ESP3连接中断");
  assert(grass_offline.fingerprint == "aquarium:esp3:connectivity:offline");

  const auto scoped_bark = aquarium::transport::bark_message(
      aquarium::transport::ScopedTemperatureEvent{
          "laosi_tank", "老四缸", event},
      "esp1");
  assert(scoped_bark.title == "【注意·首次】老四缸｜高温");
  assert(scoped_bark.body.find("情况：当时水温：27.6°C") !=
         std::string::npos);
  assert(scoped_bark.fingerprint ==
         "aquarium:esp1:laosi_tank:high_temperature");

  const auto low_critical = aquarium::transport::bark_message(
      aquarium::transport::ScopedTemperatureEvent{
          "maomao_tank", "毛毛缸",
          TemperatureEvent{EventType::low_temperature_critical,
                           EventState::opened, Severity::n3, 930000, 14.125}},
      "esp1");
  assert(low_critical.title == "【严重·首次】毛毛缸｜严重低温");
  assert(low_critical.body.find("情况：当时水温：14.1°C") !=
         std::string::npos);

  const auto high = aquarium::transport::bark_message(
      TemperatureEvent{EventType::high_temperature, EventState::opened,
                       Severity::n2, 930000, 28.16},
      "esp1");
  assert(high.title == "【注意·首次】包包缸·主缸｜高温");
  assert(high.body.find("情况：当时水温：28.2°C") != std::string::npos);

  const auto high_critical = aquarium::transport::bark_message(
      TemperatureEvent{EventType::high_temperature_critical,
                       EventState::resolved, Severity::n3, 930000, 26.5},
      "esp1");
  assert(high_critical.title == "【信息·恢复】包包缸·主缸｜严重高温已恢复");
  assert(high_critical.body.find("情况：当时水温：26.5°C") !=
         std::string::npos);

  const auto rapid_change = aquarium::transport::bark_message(
      aquarium::transport::ScopedTemperatureEvent{
          "xiaohei_tank", "小黑缸",
          TemperatureEvent{EventType::temperature_rapid_change,
                           EventState::escalated, Severity::n3, 930000, 24.16}},
      "esp1");
  assert(rapid_change.title == "【严重·升级】小黑缸｜严重温度变化");
  assert(rapid_change.body.find("情况：当时水温：24.2°C") !=
         std::string::npos);

  const auto rapid_change_resolved = aquarium::transport::bark_message(
      TemperatureEvent{EventType::temperature_rapid_change,
                       EventState::resolved, Severity::n3, 930000, 24.0},
      "esp1");
  assert(rapid_change_resolved.title ==
         "【信息·恢复】包包缸·主缸｜严重温度变化已恢复");
  assert(rapid_change_resolved.body.find("情况：当时水温：24.0°C") !=
         std::string::npos);

  const auto gradient = aquarium::transport::bark_message(
      TemperatureEvent{EventType::temperature_gradient, EventState::opened,
                       Severity::n2, 930000, 26.16},
      "esp1");
  assert(gradient.title == "【注意·首次】包包缸·主缸｜主缸与回水缸温差");
  assert(gradient.body.find("情况：当时主缸水温：26.2°C") !=
         std::string::npos);

  const auto scoped_fault = aquarium::transport::bark_message(
      aquarium::transport::ScopedTemperatureEvent{
          "xiaohei_tank", "小黑缸",
          TemperatureEvent{EventType::sensor_fault, EventState::opened,
                           Severity::n2, 900000, 0.0}},
      "esp1");
  assert(scoped_fault.title == "【注意·首次】小黑缸｜温度探头故障");
  assert(scoped_fault.body.find("情况：当时水温：无有效读数") !=
         std::string::npos);
  assert(scoped_fault.body.find("0C") == std::string::npos);

  const auto fault_reminder = aquarium::transport::bark_message(
      aquarium::transport::ScopedTemperatureEvent{
          "xiaohei_tank", "小黑缸",
          TemperatureEvent{EventType::sensor_fault, EventState::reminder,
                           Severity::n2, 930000, 0.0}},
      "esp1");
  assert(fault_reminder.title == "【注意·重复】小黑缸｜温度探头故障未解除");
  assert(fault_reminder.body.find("时间：提醒判定：时间未同步") !=
         std::string::npos);

  const auto fault_resolved = aquarium::transport::bark_message(
      TemperatureEvent{EventType::sensor_fault, EventState::resolved,
                       Severity::n2, 930000, 24.5},
      "esp1");
  assert(fault_resolved.title == "【信息·恢复】包包缸·主缸｜温度探头故障已恢复");
  assert(fault_resolved.body.find("探头读数已恢复") != std::string::npos);

  aquarium::transport::BarkAlertPolicy alert_policy;
  const auto first_alert = alert_policy.prepare(event, "main_tank");
  assert(first_alert.has_value());
  assert(first_alert->notification_number == 1U);
  const auto ordinary_reminder = alert_policy.prepare(
      TemperatureEvent{EventType::high_temperature, EventState::reminder,
                       Severity::n2, 4500000, 27.8},
      "main_tank");
  assert(!ordinary_reminder.has_value());
  const auto critical_alert = alert_policy.prepare(
      TemperatureEvent{EventType::high_temperature_critical, EventState::opened,
                       Severity::n3, 4500000, 28.8},
      "main_tank");
  assert(critical_alert.has_value());
  assert(critical_alert->state == EventState::escalated);
  assert(critical_alert->notification_number == 2U);
  const auto critical_repeat_one = alert_policy.prepare(
      TemperatureEvent{EventType::high_temperature_critical, EventState::reminder,
                       Severity::n3, 8100000, 28.8},
      "main_tank");
  const auto critical_repeat_two = alert_policy.prepare(
      TemperatureEvent{EventType::high_temperature_critical, EventState::reminder,
                       Severity::n3, 11700000, 28.8},
      "main_tank");
  const auto critical_repeat_three = alert_policy.prepare(
      TemperatureEvent{EventType::high_temperature_critical, EventState::reminder,
                       Severity::n3, 15300000, 28.8},
      "main_tank");
  assert(critical_repeat_one.has_value());
  assert(critical_repeat_two.has_value());
  assert(critical_repeat_one->repeat_number == 1U);
  assert(critical_repeat_two->repeat_number == 2U);
  assert(!critical_repeat_three.has_value());
  const auto resolved = alert_policy.prepare(
      TemperatureEvent{EventType::high_temperature_critical,
                       EventState::resolved, Severity::n3, 18900000, 27.0},
      "main_tank");
  assert(resolved.has_value());
  assert(resolved->notification_number == 0U);

  const auto formatted_body = aquarium::transport::bark_message(
      *first_alert, "esp1").body;
  assert(formatted_body.find("建议：") ==
         formatted_body.rfind("建议："));

  const auto bark_request =
      aquarium::transport::bark_request_json(bark, "unit-test-key");
  assert(bark_request.find("\"device_key\":\"unit-test-key\"") !=
         std::string::npos);
  assert(bark_request.find("\"title\":\"【注意·首次】包包缸·主缸｜高温\"") !=
         std::string::npos);

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

  const aquarium::transport::ScopedTemperatureEvent old_four_event{
      "laosi_tank", "老四缸", event};
  const aquarium::transport::ScopedTemperatureEvent xiaohei_event{
      "xiaohei_tank", "小黑缸", event};
  aquarium::transport::ScopedEventOutbox scoped_outbox(2);
  assert(scoped_outbox.push(old_four_event));
  assert(scoped_outbox.push(xiaohei_event));
  assert(!scoped_outbox.push(old_four_event));
  assert(scoped_outbox.dropped_count() == 1);
  assert(scoped_outbox.front()->tank_key == "laosi_tank");
  scoped_outbox.pop();
  assert(scoped_outbox.front()->tank_key == "xiaohei_tank");

  aquarium::transport::ScopedEventOutbox scoped_recovery(3);
  assert(scoped_recovery.push(old_four_event));
  assert(scoped_recovery.push(xiaohei_event));
  assert(scoped_recovery.push(
      {"laosi_tank", "老四缸",
       {EventType::high_temperature, EventState::resolved, Severity::n2,
        950000, 24.0}}));
  assert(scoped_recovery.front()->tank_key == "xiaohei_tank");

  aquarium::DeliveryCoordinator delivery(2, true, true);
  const std::time_t event_time = 1'750'000'000;
  assert(delivery.enqueue(event, event_time));
  assert(delivery.bark_front() != nullptr);
  assert(delivery.bark_record_front()->event_time == event_time);
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

  aquarium::DeliveryCoordinator recovery_clears_pending(2, true, false);
  assert(recovery_clears_pending.enqueue(event));
  assert(recovery_clears_pending.enqueue(
      TemperatureEvent{EventType::high_temperature, EventState::resolved,
                       Severity::n2, 950000, 24.0}));
  assert(recovery_clears_pending.bark_front()->state ==
         EventState::resolved);

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
