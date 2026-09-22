#pragma once

#include "transport_contract.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace aquarium::transport {

struct LocalDateTime {
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  bool valid = false;
};

enum class SummaryReadingState {
  valid,
  invalid,
  unconfigured,
  configuration_error,
};

enum class TemperatureReadingStatus {
  invalid,
  normal,
  low,
  low_critical,
  high,
  high_critical,
};

constexpr std::size_t kExtensionTankCount = 2U;

struct ExtensionTankReading {
  SummaryReadingState state = SummaryReadingState::invalid;
  std::optional<double> temperature_c;
  TemperatureReadingStatus status = TemperatureReadingStatus::invalid;
  bool fresh = true;
};

struct DailyTemperatureSnapshot {
  LocalDateTime sampled_at;
  std::optional<double> main_c;
  std::optional<double> sump_c;
  TemperatureReadingStatus main_status = TemperatureReadingStatus::invalid;
  std::array<std::optional<double>, 3> auxiliary_c{};
  std::array<TemperatureReadingStatus, 3> auxiliary_status{
      TemperatureReadingStatus::invalid, TemperatureReadingStatus::invalid,
      TemperatureReadingStatus::invalid};
  std::array<SummaryReadingState, 3> auxiliary_states{
      SummaryReadingState::invalid, SummaryReadingState::invalid,
      SummaryReadingState::invalid};
  std::array<ExtensionTankReading, kExtensionTankCount> extension_tanks{};
};

enum class DailySlot {
  morning,
  evening,
};

struct DailyTemperatureSummary {
  std::uint32_t slot_key = 0;
  DailySlot slot = DailySlot::morning;
  DailyTemperatureSnapshot snapshot;
};

class DailySummaryScheduler {
 public:
  std::optional<DailyTemperatureSummary> observe(
      const LocalDateTime& now, std::uint64_t monotonic_now_ms,
      const DailyTemperatureSnapshot& snapshot);

  const DailyTemperatureSummary* pending() const;
  void acknowledge(bool delivered);
  void expire(std::uint64_t monotonic_now_ms);

 private:
  std::optional<std::uint32_t> last_slot_key_;
  std::optional<DailyTemperatureSummary> pending_;
  std::uint64_t pending_expires_at_ms_ = 0;
};

std::string daily_summary_body(const DailyTemperatureSummary& summary);
TemperatureReadingStatus temperature_reading_status(
    const std::optional<double>& value, const TemperaturePolicy& policy);
BarkMessage daily_summary_message(const DailyTemperatureSummary& summary,
                                  const std::string& aquarium_id);

}  // namespace aquarium::transport
