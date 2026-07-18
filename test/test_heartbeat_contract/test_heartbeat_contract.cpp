#include "heartbeat_contract.hpp"

#include <cassert>
#include <iostream>
#include <string>

int main() {
  using aquarium::heartbeat::HeartbeatPayload;
  using aquarium::heartbeat::HeartbeatSchedule;

  const HeartbeatPayload payload{
      "tank01",
      1750000000123ULL,
      "00112233445566778899aabbccddeeff",
      26.4,
      26.75,
      123456ULL,
  };

  const auto body = aquarium::heartbeat::heartbeat_json(payload);
  assert(body ==
         "{\"device_id\":\"tank01\",\"sent_at_ms\":1750000000123,"
         "\"nonce\":\"00112233445566778899aabbccddeeff\","
         "\"main_c\":26.4,\"sump_c\":26.75,\"uptime_ms\":123456}");

  const auto escaped = aquarium::heartbeat::heartbeat_json(
      HeartbeatPayload{"tank\\\"01", 1, "0011223344556677", -0.25, 30.0, 2});
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
