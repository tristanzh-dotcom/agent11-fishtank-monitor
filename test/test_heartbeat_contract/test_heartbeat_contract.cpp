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
       aquarium::Severity::n2, 1, 26.0},
      {aquarium::EventType::high_temperature_critical,
       aquarium::EventState::opened, aquarium::Severity::n3, 2, 29.0},
      {aquarium::EventType::low_temperature, aquarium::EventState::reminder,
       aquarium::Severity::n2, 3, 23.0},
      {aquarium::EventType::low_temperature_critical,
       aquarium::EventState::opened, aquarium::Severity::n3, 4, 22.0},
      {aquarium::EventType::sensor_fault, aquarium::EventState::opened,
       aquarium::Severity::n2, 5, std::nullopt},
      {aquarium::EventType::temperature_rapid_change,
       aquarium::EventState::escalated, aquarium::Severity::n3, 6, 28.0},
      {aquarium::EventType::temperature_gradient,
       aquarium::EventState::reminder, aquarium::Severity::n2, 7, 26.0},
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

  const std::string body_hash(
      "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");
  assert(aquarium::heartbeat::signature_canonical(payload, body_hash) ==
         "v1\n1750000000123\n00112233445566778899aabbccddeeff\n" +
             body_hash);

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
