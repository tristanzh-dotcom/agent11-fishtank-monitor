#pragma once

#include <cstdint>

namespace aquarium {

class RetryBackoff {
 public:
  RetryBackoff(std::uint64_t initial_delay_ms, std::uint64_t max_delay_ms)
      : initial_delay_ms_(initial_delay_ms), max_delay_ms_(max_delay_ms) {}

  bool should_attempt(std::uint64_t now_ms) const;
  void record_failure(std::uint64_t now_ms);
  void record_success();

 private:
  std::uint64_t initial_delay_ms_;
  std::uint64_t max_delay_ms_;
  std::uint64_t current_delay_ms_ = 0;
  std::uint64_t next_attempt_at_ms_ = 0;
};

}  // namespace aquarium
