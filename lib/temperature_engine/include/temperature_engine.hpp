#pragma once

#include <cstdint>
#include <deque>
#include <optional>
#include <vector>

namespace aquarium {

enum class EventType {
  high_temperature,
  high_temperature_critical,
  low_temperature,
  low_temperature_critical,
  sensor_fault,
  temperature_rapid_change,
  temperature_gradient,
};

enum class EventState {
  opened,
  escalated,
  reminder,
  resolved,
};

enum class Severity {
  n2,
  n3,
};

struct TemperatureSample {
  std::uint64_t at_ms;
  std::optional<double> display_c;
  std::optional<double> return_c;
};

struct TemperaturePolicy {
  double low_attention_c = 23.5;
  double low_critical_c = 22.5;
  double high_attention_c = 27.5;
  double high_critical_c = 28.5;
  double low_recovery_c = 24.5;
  double high_recovery_c = 26.5;
  double rapid_change_c = 1.0;
  double rapid_change_critical_c = 2.0;
  double gradient_c = 0.8;
  double gradient_recovery_c = 0.6;
  std::uint64_t attention_duration_ms = 15U * 60U * 1000U;
  std::uint64_t critical_duration_ms = 5U * 60U * 1000U;
  std::uint64_t recovery_duration_ms = 15U * 60U * 1000U;
  std::uint64_t rapid_change_window_ms = 30U * 60U * 1000U;
  std::uint64_t rapid_change_recovery_duration_ms = 30U * 60U * 1000U;
  std::uint64_t gradient_duration_ms = 10U * 60U * 1000U;
  std::uint64_t gradient_recovery_duration_ms = 10U * 60U * 1000U;
  std::uint64_t reminder_interval_ms = 60U * 60U * 1000U;
};

struct TemperatureEvent {
  EventType type;
  EventState state;
  Severity severity;
  std::uint64_t at_ms;
  double display_c;
};

class TemperatureEngine {
 public:
  explicit TemperatureEngine(TemperaturePolicy policy = {});
  std::vector<TemperatureEvent> ingest(const TemperatureSample& sample);

 private:
  TemperaturePolicy policy_;
  std::optional<std::uint64_t> high_candidate_started_at_ms_;
  std::optional<std::uint64_t> high_critical_candidate_started_at_ms_;
  std::optional<std::uint64_t> high_recovery_started_at_ms_;
  bool high_open_ = false;
  bool high_critical_open_ = false;
  std::optional<std::uint64_t> high_last_notified_at_ms_;
  std::optional<std::uint64_t> high_critical_last_notified_at_ms_;

  std::optional<std::uint64_t> low_candidate_started_at_ms_;
  std::optional<std::uint64_t> low_critical_candidate_started_at_ms_;
  std::optional<std::uint64_t> low_recovery_started_at_ms_;
  bool low_open_ = false;
  bool low_critical_open_ = false;
  std::optional<std::uint64_t> low_last_notified_at_ms_;
  std::optional<std::uint64_t> low_critical_last_notified_at_ms_;

  std::uint8_t consecutive_invalid_display_samples_ = 0;
  bool sensor_fault_open_ = false;
  std::optional<std::uint64_t> sensor_fault_last_notified_at_ms_;

  std::deque<TemperatureSample> display_history_;
  bool rapid_change_open_ = false;
  Severity rapid_change_peak_severity_ = Severity::n2;
  std::optional<std::uint64_t> rapid_change_recovery_started_at_ms_;
  std::optional<std::uint64_t> rapid_change_last_notified_at_ms_;

  std::optional<std::uint64_t> gradient_candidate_started_at_ms_;
  bool gradient_open_ = false;
  std::optional<std::uint64_t> gradient_recovery_started_at_ms_;
  std::optional<std::uint64_t> gradient_last_notified_at_ms_;
};

}  // namespace aquarium
