#include "retry_backoff.hpp"

namespace aquarium {

bool RetryBackoff::should_attempt(std::uint64_t now_ms) const {
  return now_ms >= next_attempt_at_ms_;
}

void RetryBackoff::record_failure(std::uint64_t now_ms) {
  current_delay_ms_ = current_delay_ms_ == 0
                          ? initial_delay_ms_
                          : (current_delay_ms_ >= max_delay_ms_ / 2U
                                 ? max_delay_ms_
                                 : current_delay_ms_ * 2U);
  next_attempt_at_ms_ = now_ms + current_delay_ms_;
}

void RetryBackoff::record_success() {
  current_delay_ms_ = 0;
  next_attempt_at_ms_ = 0;
}

}  // namespace aquarium
