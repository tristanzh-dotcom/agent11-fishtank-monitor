#pragma once

#include <cstdint>
#include <string>

namespace aquarium::heartbeat {

struct HeartbeatPayload {
  std::string device_id;
  std::uint64_t sent_at_ms;
  std::string nonce;
  double main_c;
  double sump_c;
  std::uint64_t uptime_ms;
};

std::string heartbeat_json(const HeartbeatPayload& payload);
std::string signature_canonical(const HeartbeatPayload& payload,
                                const std::string& raw_body_sha256_hex);
bool valid_nonce(const std::string& nonce);

class HeartbeatSchedule {
 public:
  HeartbeatSchedule(std::uint64_t nominal_interval_ms,
                    std::uint64_t initial_retry_ms,
                    std::uint64_t maximum_retry_ms);

  bool should_attempt(std::uint64_t now_ms) const;
  void record_failure(std::uint64_t now_ms);
  void record_success(std::uint64_t now_ms);

 private:
  std::uint64_t nominal_interval_ms_;
  std::uint64_t initial_retry_ms_;
  std::uint64_t maximum_retry_ms_;
  std::uint64_t current_retry_ms_ = 0;
  std::uint64_t next_attempt_at_ms_ = 0;
};

}  // namespace aquarium::heartbeat

