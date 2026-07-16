#include "temperature_engine.hpp"

#include <cmath>

namespace aquarium {
namespace {

bool has_elapsed(std::uint64_t now_ms, std::uint64_t started_at_ms,
                 std::uint64_t duration_ms) {
  return now_ms >= started_at_ms && now_ms - started_at_ms >= duration_ms;
}

}  // namespace

TemperatureEngine::TemperatureEngine(TemperaturePolicy policy)
    : policy_(policy) {}

std::vector<TemperatureEvent> TemperatureEngine::ingest(
    const TemperatureSample& sample) {
  std::vector<TemperatureEvent> events;
  if (!sample.display_c.has_value()) {
    ++consecutive_invalid_display_samples_;
    if (!sensor_fault_open_ && consecutive_invalid_display_samples_ >= 2U) {
      sensor_fault_open_ = true;
    events.push_back({EventType::sensor_fault, EventState::opened,
                        Severity::n2, sample.at_ms, 0.0});
    }
    return events;
  }

  const double display_c = *sample.display_c;
  consecutive_invalid_display_samples_ = 0;
  if (sensor_fault_open_) {
    sensor_fault_open_ = false;
    events.push_back({EventType::sensor_fault, EventState::resolved,
                      Severity::n2, sample.at_ms, display_c});
  }

  if (display_c > policy_.high_critical_c) {
    if (!high_critical_candidate_started_at_ms_.has_value()) {
      high_critical_candidate_started_at_ms_ = sample.at_ms;
    }
    if (!high_critical_open_ && has_elapsed(sample.at_ms,
                                             *high_critical_candidate_started_at_ms_,
                                             policy_.critical_duration_ms)) {
      high_critical_open_ = true;
      high_candidate_started_at_ms_.reset();
      high_critical_last_notified_at_ms_ = sample.at_ms;
      events.push_back({EventType::high_temperature_critical, EventState::opened,
                        Severity::n3, sample.at_ms, display_c});
    }
    if (high_critical_open_ && high_critical_last_notified_at_ms_.has_value() &&
        has_elapsed(sample.at_ms, *high_critical_last_notified_at_ms_,
                    policy_.reminder_interval_ms)) {
      high_critical_last_notified_at_ms_ = sample.at_ms;
      events.push_back({EventType::high_temperature_critical,
                        EventState::reminder, Severity::n3, sample.at_ms,
                        display_c});
    }
  } else {
    high_critical_candidate_started_at_ms_.reset();
  }

  if (!high_critical_open_ && display_c > policy_.high_attention_c) {
    if (!high_candidate_started_at_ms_.has_value()) {
      high_candidate_started_at_ms_ = sample.at_ms;
    }
    if (!high_open_ && has_elapsed(sample.at_ms, *high_candidate_started_at_ms_,
                                    policy_.attention_duration_ms)) {
      high_open_ = true;
      high_last_notified_at_ms_ = sample.at_ms;
      events.push_back({EventType::high_temperature, EventState::opened,
                        Severity::n2, sample.at_ms, display_c});
    }
    if (high_open_ && high_last_notified_at_ms_.has_value() &&
        has_elapsed(sample.at_ms, *high_last_notified_at_ms_,
                    policy_.reminder_interval_ms)) {
      high_last_notified_at_ms_ = sample.at_ms;
      events.push_back({EventType::high_temperature, EventState::reminder,
                        Severity::n2, sample.at_ms, display_c});
    }
  } else if (display_c <= policy_.high_attention_c) {
    high_candidate_started_at_ms_.reset();
  }

  if ((high_open_ || high_critical_open_) && display_c <= policy_.high_recovery_c) {
    if (!high_recovery_started_at_ms_.has_value()) {
      high_recovery_started_at_ms_ = sample.at_ms;
    }
    if (has_elapsed(sample.at_ms, *high_recovery_started_at_ms_,
                    policy_.recovery_duration_ms)) {
      if (high_critical_open_) {
        events.push_back({EventType::high_temperature_critical,
                          EventState::resolved, Severity::n3, sample.at_ms,
                          display_c});
      }
      if (high_open_) {
        events.push_back({EventType::high_temperature, EventState::resolved,
                          Severity::n2, sample.at_ms, display_c});
      }
      high_open_ = false;
      high_critical_open_ = false;
      high_last_notified_at_ms_.reset();
      high_critical_last_notified_at_ms_.reset();
      high_recovery_started_at_ms_.reset();
    }
  } else {
    high_recovery_started_at_ms_.reset();
  }

  if (display_c < policy_.low_critical_c) {
    if (!low_critical_candidate_started_at_ms_.has_value()) {
      low_critical_candidate_started_at_ms_ = sample.at_ms;
    }
    if (!low_critical_open_ && has_elapsed(sample.at_ms,
                                            *low_critical_candidate_started_at_ms_,
                                            policy_.critical_duration_ms)) {
      low_critical_open_ = true;
      low_candidate_started_at_ms_.reset();
      low_critical_last_notified_at_ms_ = sample.at_ms;
      events.push_back({EventType::low_temperature_critical, EventState::opened,
                        Severity::n3, sample.at_ms, display_c});
    }
    if (low_critical_open_ && low_critical_last_notified_at_ms_.has_value() &&
        has_elapsed(sample.at_ms, *low_critical_last_notified_at_ms_,
                    policy_.reminder_interval_ms)) {
      low_critical_last_notified_at_ms_ = sample.at_ms;
      events.push_back({EventType::low_temperature_critical,
                        EventState::reminder, Severity::n3, sample.at_ms,
                        display_c});
    }
  } else {
    low_critical_candidate_started_at_ms_.reset();
  }

  if (!low_critical_open_ && display_c < policy_.low_attention_c) {
    if (!low_candidate_started_at_ms_.has_value()) {
      low_candidate_started_at_ms_ = sample.at_ms;
    }
    if (!low_open_ && has_elapsed(sample.at_ms, *low_candidate_started_at_ms_,
                                   policy_.attention_duration_ms)) {
      low_open_ = true;
      low_last_notified_at_ms_ = sample.at_ms;
      events.push_back({EventType::low_temperature, EventState::opened,
                        Severity::n2, sample.at_ms, display_c});
    }
    if (low_open_ && low_last_notified_at_ms_.has_value() &&
        has_elapsed(sample.at_ms, *low_last_notified_at_ms_,
                    policy_.reminder_interval_ms)) {
      low_last_notified_at_ms_ = sample.at_ms;
      events.push_back({EventType::low_temperature, EventState::reminder,
                        Severity::n2, sample.at_ms, display_c});
    }
  } else if (display_c >= policy_.low_attention_c) {
    low_candidate_started_at_ms_.reset();
  }

  if ((low_open_ || low_critical_open_) && display_c >= policy_.low_recovery_c) {
    if (!low_recovery_started_at_ms_.has_value()) {
      low_recovery_started_at_ms_ = sample.at_ms;
    }
    if (has_elapsed(sample.at_ms, *low_recovery_started_at_ms_,
                    policy_.recovery_duration_ms)) {
      if (low_critical_open_) {
        events.push_back({EventType::low_temperature_critical,
                          EventState::resolved, Severity::n3, sample.at_ms,
                          display_c});
      }
      if (low_open_) {
        events.push_back({EventType::low_temperature, EventState::resolved,
                          Severity::n2, sample.at_ms, display_c});
      }
      low_open_ = false;
      low_critical_open_ = false;
      low_last_notified_at_ms_.reset();
      low_critical_last_notified_at_ms_.reset();
      low_recovery_started_at_ms_.reset();
    }
  } else {
    low_recovery_started_at_ms_.reset();
  }

  while (!display_history_.empty() &&
         sample.at_ms - display_history_.front().at_ms >
             policy_.rapid_change_window_ms) {
    display_history_.pop_front();
  }
  if (!display_history_.empty() &&
      sample.at_ms - display_history_.front().at_ms >= policy_.rapid_change_window_ms &&
      std::fabs(display_c - *display_history_.front().display_c) >=
          policy_.rapid_change_c &&
      !rapid_change_open_) {
    rapid_change_open_ = true;
    const bool critical =
        std::fabs(display_c - *display_history_.front().display_c) >=
        policy_.rapid_change_critical_c;
    events.push_back({critical ? EventType::temperature_rapid_change_critical
                               : EventType::temperature_rapid_change,
                      EventState::opened, critical ? Severity::n3 : Severity::n2,
                      sample.at_ms, display_c});
  }
  display_history_.push_back(sample);

  if (sample.return_c.has_value() &&
      std::fabs(display_c - *sample.return_c) > policy_.gradient_c) {
    if (!gradient_candidate_started_at_ms_.has_value()) {
      gradient_candidate_started_at_ms_ = sample.at_ms;
    }
    if (!gradient_open_ &&
        has_elapsed(sample.at_ms, *gradient_candidate_started_at_ms_,
                    policy_.gradient_duration_ms)) {
      gradient_open_ = true;
      events.push_back({EventType::temperature_gradient, EventState::opened,
                        Severity::n2, sample.at_ms, display_c});
    }
  } else {
    gradient_candidate_started_at_ms_.reset();
  }

  return events;
}

}  // namespace aquarium
