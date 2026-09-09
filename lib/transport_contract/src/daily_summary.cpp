#include "daily_summary.hpp"

#include <cstdio>
#include <limits>
#include <string>

namespace aquarium::transport {
namespace {

constexpr std::uint64_t kSummaryRetryWindowMs = 10U * 60U * 1000U;

bool valid_date_time(const LocalDateTime& value) {
  return value.valid && value.year >= 1970 && value.year <= 9999 &&
         value.month >= 1 && value.month <= 12 && value.day >= 1 &&
         value.day <= 31 && value.hour >= 0 && value.hour <= 23 &&
         value.minute >= 0 && value.minute <= 59 && value.second >= 0 &&
         value.second <= 59;
}

std::optional<DailySlot> slot_for(const LocalDateTime& value) {
  if (!valid_date_time(value) || value.minute != 0) {
    return std::nullopt;
  }
  if (value.hour == 9) {
    return DailySlot::morning;
  }
  if (value.hour == 18) {
    return DailySlot::evening;
  }
  return std::nullopt;
}

std::uint32_t slot_key(const LocalDateTime& value, DailySlot slot) {
  const auto date = static_cast<std::uint32_t>(value.year * 10000 +
                                               value.month * 100 + value.day);
  return date * 2U + (slot == DailySlot::evening ? 1U : 0U);
}

std::uint64_t saturating_add(std::uint64_t left, std::uint64_t right) {
  if (right > std::numeric_limits<std::uint64_t>::max() - left) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return left + right;
}

std::string temperature_text(const std::optional<double>& value) {
  if (!value.has_value()) {
    return "无有效读数";
  }
  char buffer[32]{};
  std::snprintf(buffer, sizeof(buffer), "%.2f°C", *value);
  return buffer;
}

std::string local_time_text(const LocalDateTime& value) {
  char buffer[64]{};
  std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d（上海时间）",
                value.year, value.month, value.day, value.hour, value.minute,
                value.second);
  return buffer;
}

std::string auxiliary_text(const DailyTemperatureSnapshot& snapshot,
                           std::size_t index) {
  switch (snapshot.auxiliary_states[index]) {
    case SummaryReadingState::valid:
      return temperature_text(snapshot.auxiliary_c[index]);
    case SummaryReadingState::invalid:
      return "无有效读数";
    case SummaryReadingState::unconfigured:
      return "未配置";
    case SummaryReadingState::configuration_error:
      return "配置错误";
  }
  return "无有效读数";
}

const char* slot_name(DailySlot slot) {
  return slot == DailySlot::morning ? "morning" : "evening";
}

}  // namespace

std::optional<DailyTemperatureSummary> DailySummaryScheduler::observe(
    const LocalDateTime& now, std::uint64_t monotonic_now_ms,
    const DailyTemperatureSnapshot& snapshot) {
  expire(monotonic_now_ms);
  const auto slot = slot_for(now);
  if (!slot.has_value() || pending_.has_value()) {
    return std::nullopt;
  }

  const auto key = slot_key(now, *slot);
  if (last_slot_key_.has_value() && key <= *last_slot_key_) {
    return std::nullopt;
  }

  last_slot_key_ = key;
  pending_ = DailyTemperatureSummary{key, *slot, snapshot};
  pending_expires_at_ms_ = saturating_add(monotonic_now_ms,
                                          kSummaryRetryWindowMs);
  return pending_;
}

const DailyTemperatureSummary* DailySummaryScheduler::pending() const {
  return pending_.has_value() ? &*pending_ : nullptr;
}

void DailySummaryScheduler::acknowledge(bool delivered) {
  if (delivered) {
    pending_.reset();
    pending_expires_at_ms_ = 0;
  }
}

void DailySummaryScheduler::expire(std::uint64_t monotonic_now_ms) {
  if (pending_.has_value() && monotonic_now_ms >= pending_expires_at_ms_) {
    pending_.reset();
    pending_expires_at_ms_ = 0;
  }
}

std::string daily_summary_body(const DailyTemperatureSummary& summary) {
  const auto& snapshot = summary.snapshot;
  std::string body = "采样时间：" + local_time_text(snapshot.sampled_at) +
                     "\n\n包包缸：" + temperature_text(snapshot.main_c) +
                     "\n回水缸：" + temperature_text(snapshot.sump_c) +
                     "\n老四缸：" + auxiliary_text(snapshot, 0) +
                     "\n小黑缸：" + auxiliary_text(snapshot, 1) +
                     "\n毛毛缸：" + auxiliary_text(snapshot, 2);
  return body;
}

BarkMessage daily_summary_message(const DailyTemperatureSummary& summary,
                                  const std::string& aquarium_id) {
  const auto& sampled_at = summary.snapshot.sampled_at;
  const auto date = static_cast<std::uint32_t>(sampled_at.year * 10000 +
                                               sampled_at.month * 100 +
                                               sampled_at.day);
  return BarkMessage{
      "鱼缸温度提醒", daily_summary_body(summary), "aquarium-daily", "active",
      "aquarium:" + aquarium_id + ":" + std::to_string(date) + ":" +
          slot_name(summary.slot)};
}

}  // namespace aquarium::transport
