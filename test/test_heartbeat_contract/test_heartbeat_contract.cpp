#include "active_event_snapshot.hpp"
#include "heartbeat_contract.hpp"

#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

int main() {
  using aquarium::heartbeat::HeartbeatPayload;
  using aquarium::heartbeat::HeartbeatSchedule;

  const HeartbeatPayload payload{
      "tank01",
      1750000000123ULL,
      "00112233445566778899aabbccddeeff",
      std::optional<double>{26.4},
      std::optional<double>{26.75},
      123456ULL,
      {aquarium::ActiveTemperatureEvent{aquarium::EventType::high_temperature,
                                        aquarium::EventState::escalated,
                                        aquarium::Severity::n3, 123000ULL,
                                        std::optional<double>{28.75}}},
  };

  const auto body = aquarium::heartbeat::heartbeat_json(payload);
  assert(body ==
         "{\"device_id\":\"tank01\",\"sent_at_ms\":1750000000123,"
         "\"nonce\":\"00112233445566778899aabbccddeeff\","
         "\"main_c\":26.4,\"sump_c\":26.75,\"uptime_ms\":123456,"
         "\"active_events\":[{\"type\":\"high_temperature\","
         "\"state\":\"escalated\",\"severity\":\"n3\","
         "\"at_ms\":123000,\"display_c\":28.75}]}");

  const auto fault_body = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{"tank01", 1, "0011223344556677", std::nullopt,
                       std::nullopt, 2,
                       {aquarium::ActiveTemperatureEvent{
                           aquarium::EventType::sensor_fault,
                           aquarium::EventState::opened,
                           aquarium::Severity::n2,
                           1,
                           std::nullopt}}});
  assert(fault_body.find("\"main_c\":null") != std::string::npos);
  assert(fault_body.find("\"sump_c\":null") != std::string::npos);
  assert(fault_body.find("\"display_c\":null") != std::string::npos);

  const std::vector<aquarium::ActiveTemperatureEvent> all_events{
      {aquarium::EventType::high_temperature, aquarium::EventState::opened,
       aquarium::Severity::n2, 1750000000000ULL, 60.0},
      {aquarium::EventType::high_temperature_critical,
       aquarium::EventState::opened, aquarium::Severity::n3, 1750000000000ULL,
       60.0},
      {aquarium::EventType::low_temperature, aquarium::EventState::reminder,
       aquarium::Severity::n2, 1750000000000ULL, -20.0},
      {aquarium::EventType::low_temperature_critical,
       aquarium::EventState::opened, aquarium::Severity::n3, 1750000000000ULL,
       -20.0},
      {aquarium::EventType::sensor_fault, aquarium::EventState::opened,
       aquarium::Severity::n2, 1750000000000ULL, std::nullopt},
      {aquarium::EventType::temperature_rapid_change,
       aquarium::EventState::escalated, aquarium::Severity::n3, 1750000000000ULL,
       60.0},
      {aquarium::EventType::temperature_gradient,
       aquarium::EventState::reminder, aquarium::Severity::n2, 1750000000000ULL,
       60.0},
  };
  assert(aquarium::heartbeat::heartbeat_json(
             HeartbeatPayload{"tank01", 1, "0011223344556677", 26.0,
                              26.0, 1, all_events})
             .size() <= 2048U);

  const auto escaped = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{"tank\\\"01", 1, "0011223344556677", -0.25, 30.0,
                       2, {}});
  assert(escaped.find("\"device_id\":\"tank\\\\\\\"01\"") !=
         std::string::npos);
  assert(escaped.find("\"main_c\":-0.25") != std::string::npos);
  assert(escaped.find("\"sump_c\":30") != std::string::npos);

  const auto with_snapshot = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{
          "tank01", 1750000000123ULL, "00112233445566778899aabbccddeeff",
          std::optional<double>{26.4}, std::optional<double>{26.75}, 123456ULL,
          {},
          aquarium::heartbeat::TemperatureSnapshot{
              1750000000000ULL, "采样时间：2025-06-15 23:06:40（上海时间）\nA\"B"},
      });
  assert(with_snapshot.find("\"temperature_snapshot\":{") !=
         std::string::npos);
  assert(with_snapshot.find("\"sampled_at_ms\":1750000000000") !=
         std::string::npos);
  assert(with_snapshot.find("\\nA\\\"B") != std::string::npos);

  const auto max_summary = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{"tank01", 1, "0011223344556677", 26.0, 26.0, 1, {},
                       aquarium::heartbeat::TemperatureSnapshot{
                           1750000000000ULL, std::string(768, 'x')}});
  assert(max_summary.find("\"temperature_snapshot\":{") !=
         std::string::npos);

  const std::string utf8_character = "\xE6\xB5\x8B";
  std::string exact_multibyte_summary;
  for (int index = 0; index < 256; ++index) {
    exact_multibyte_summary += utf8_character;
  }
  const auto max_multibyte_summary = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{"tank01", 1, "0011223344556677", 26.0, 26.0, 1, {},
                       aquarium::heartbeat::TemperatureSnapshot{
                           1750000000000ULL, exact_multibyte_summary}});
  assert(max_multibyte_summary.find("\"temperature_snapshot\":{") !=
         std::string::npos);

  const auto over_multibyte_summary = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{"tank01", 1, "0011223344556677", 26.0, 26.0, 1, {},
                       aquarium::heartbeat::TemperatureSnapshot{
                           1750000000000ULL, exact_multibyte_summary + "x"}});
  assert(over_multibyte_summary.find("\"temperature_snapshot\":{") ==
         std::string::npos);

  std::string body_limit_summary_text;
  std::string body_limit_summary;
  for (std::size_t newline_count = 0; newline_count <= 768U;
       ++newline_count) {
    std::string summary(newline_count, '\n');
    summary.append(768U - newline_count, 'x');
    const auto candidate = aquarium::heartbeat::heartbeat_json(
        HeartbeatPayload{"tank01", 1750000000123ULL,
                         "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                         60.0, -20.0, 18446744073709551615ULL, all_events,
                         aquarium::heartbeat::TemperatureSnapshot{
                             1750000000000ULL, summary}});
    if (candidate.size() == aquarium::heartbeat::kMaxHeartbeatBodyBytes) {
      body_limit_summary_text = summary;
      body_limit_summary = candidate;
      break;
    }
  }
  assert(body_limit_summary.size() ==
         aquarium::heartbeat::kMaxHeartbeatBodyBytes);
  assert(body_limit_summary.find("\"temperature_snapshot\":{") !=
         std::string::npos);
  assert(body_limit_summary.find("\"active_events\":[{") !=
         std::string::npos);

  const auto replacement = body_limit_summary_text.find('x');
  assert(replacement != std::string::npos);
  body_limit_summary_text[replacement] = '\n';
  const auto over_body = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{"tank01", 1750000000123ULL,
                       "00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff",
                       60.0, -20.0, 18446744073709551615ULL, all_events,
                       aquarium::heartbeat::TemperatureSnapshot{
                           1750000000000ULL, body_limit_summary_text}});
  assert(over_body.find("\"temperature_snapshot\":{") == std::string::npos);
  assert(over_body.find("\"active_events\":[{") != std::string::npos);

  const auto over_summary = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{"tank01", 1, "0011223344556677", 26.0, 26.0, 1, {},
                       aquarium::heartbeat::TemperatureSnapshot{
                           0ULL, std::string(768, 'x')}});
  assert(over_summary.find("\"temperature_snapshot\":{") ==
         std::string::npos);

  const std::string invalid_utf8("\xc0\xaf", 2);
  const auto invalid_utf8_summary = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{"tank01", 1, "0011223344556677", 26.0, 26.0, 1, {},
                       aquarium::heartbeat::TemperatureSnapshot{
                           1750000000000ULL, invalid_utf8}});
  assert(invalid_utf8_summary.find("\"temperature_snapshot\":{") ==
         std::string::npos);

  const auto escaped_over_body = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{"tank01", 1, "0011223344556677", 26.0, 26.0, 1,
                       all_events,
                       aquarium::heartbeat::TemperatureSnapshot{
                           1750000000000ULL, std::string(768, '\n')}});
  assert(escaped_over_body.find("\"temperature_snapshot\":{") ==
         std::string::npos);

  const std::string body_hash(
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
  assert(aquarium::heartbeat::signature_canonical(payload, body_hash) ==
         "v1\n1750000000123\n00112233445566778899aabbccddeeff\n" +
             body_hash);
  assert(with_snapshot != body);
  assert(aquarium::heartbeat::signature_canonical(payload, body_hash) !=
         aquarium::heartbeat::signature_canonical(
             payload, std::string(64, '1')));

  assert(aquarium::heartbeat::valid_nonce("0011223344556677"));
  assert(aquarium::heartbeat::valid_nonce(
      "00112233445566778899aabbccddeeff"));
  assert(!aquarium::heartbeat::valid_nonce("001122334455667"));
  assert(!aquarium::heartbeat::valid_nonce("00112233445566gg"));
  assert(!aquarium::heartbeat::valid_nonce(std::string(66, 'a')));

  HeartbeatSchedule schedule(300000U, 5000U, 300000U);
  assert(schedule.should_attempt(0));
  schedule.record_failure(0);
  assert(!schedule.should_attempt(4999));
  assert(schedule.should_attempt(5000));
  schedule.record_failure(5000);
  assert(!schedule.should_attempt(14999));
  assert(schedule.should_attempt(15000));
  schedule.record_success(15000);
  assert(!schedule.should_attempt(314999));
  assert(schedule.should_attempt(315000));

  schedule.record_failure(315000);
  assert(!schedule.should_attempt(319999));
  assert(schedule.should_attempt(320000));

  std::cout << "heartbeat contract tests passed\n";
}
