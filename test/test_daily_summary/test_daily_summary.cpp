#include "daily_summary.hpp"

#include <cassert>
#include <iostream>
#include <optional>
#include <string>

using aquarium::transport::DailySlot;
using aquarium::transport::DailySummaryScheduler;
using aquarium::transport::DailyTemperatureSnapshot;
using aquarium::transport::LocalDateTime;
using aquarium::transport::SummaryReadingState;

namespace {

DailyTemperatureSnapshot snapshot() {
  DailyTemperatureSnapshot value{};
  value.sampled_at = LocalDateTime{2026, 9, 9, 9, 0, 20, true};
  value.main_c = 25.1;
  value.sump_c = 25.0;
  value.auxiliary_c[0] = 23.4;
  value.auxiliary_c[1] = std::nullopt;
  value.auxiliary_c[2] = 24.8;
  value.auxiliary_states[0] = SummaryReadingState::valid;
  value.auxiliary_states[1] = SummaryReadingState::invalid;
  value.auxiliary_states[2] = SummaryReadingState::unconfigured;
  return value;
}

void test_daily_summary_windows_and_expiry() {
  DailySummaryScheduler scheduler;
  const auto values = snapshot();

  assert(!scheduler
              .observe(LocalDateTime{2026, 9, 9, 8, 59, 59, true}, 0, values)
              .has_value());
  assert(!scheduler
              .observe(LocalDateTime{2026, 9, 9, 9, 1, 0, true}, 500, values)
              .has_value());
  const auto created = scheduler.observe(
      LocalDateTime{2026, 9, 9, 9, 0, 20, true}, 1000, values);
  assert(created.has_value());
  assert(created->slot == DailySlot::morning);
  assert(created->snapshot.sampled_at.second == 20);
  assert(scheduler.pending() != nullptr);

  assert(!scheduler
              .observe(LocalDateTime{2026, 9, 9, 9, 0, 50, true}, 2000, values)
              .has_value());
  scheduler.acknowledge(false);
  assert(scheduler.pending() != nullptr);
  scheduler.expire(1000 + 600000 - 1);
  assert(scheduler.pending() != nullptr);
  scheduler.expire(1000 + 600000);
  assert(scheduler.pending() == nullptr);
  assert(!scheduler
              .observe(LocalDateTime{2026, 9, 9, 9, 0, 55, true}, 700000,
                       values)
              .has_value());
}

void test_daily_summary_rejects_invalid_time_and_handles_slot_order() {
  DailySummaryScheduler scheduler;
  const auto values = snapshot();

  assert(!scheduler
              .observe(LocalDateTime{2026, 9, 9, 9, 0, 0, false}, 0, values)
              .has_value());
  assert(!scheduler
              .observe(LocalDateTime{2026, 9, 9, 9, 2, 0, true}, 1000, values)
              .has_value());
  assert(scheduler
             .observe(LocalDateTime{2026, 9, 9, 18, 0, 0, true}, 2000, values)
             .has_value());
  scheduler.acknowledge(true);
  assert(scheduler.pending() == nullptr);
  assert(!scheduler
              .observe(LocalDateTime{2026, 9, 9, 9, 0, 30, true}, 3000, values)
              .has_value());
  assert(scheduler
             .observe(LocalDateTime{2026, 10, 10, 9, 0, 0, true}, 4000,
                      values)
             .has_value());
}

void test_daily_summary_body_reports_all_five_channels_and_states() {
  DailySummaryScheduler scheduler;
  const auto created = scheduler.observe(
      LocalDateTime{2026, 9, 9, 9, 0, 20, true}, 1000, snapshot());
  assert(created.has_value());

  const auto body = aquarium::transport::daily_summary_body(*created);
  assert(body.find("采样时间：2026-09-09 09:00:20") != std::string::npos);
  assert(body.find("包包缸：25.10°C") != std::string::npos);
  assert(body.find("回水缸：25.00°C") != std::string::npos);
  assert(body.find("老四缸：23.40°C") != std::string::npos);
  assert(body.find("小黑缸：无有效读数") != std::string::npos);
  assert(body.find("毛毛缸：未配置") != std::string::npos);

  const auto message =
      aquarium::transport::daily_summary_message(*created, "tank01");
  assert(message.title == "鱼缸温度提醒");
  assert(message.group == "aquarium-daily");
  assert(message.level == "active");
  assert(message.fingerprint == "aquarium:tank01:20260909:morning");
}

}  // namespace

int main() {
  test_daily_summary_windows_and_expiry();
  test_daily_summary_rejects_invalid_time_and_handles_slot_order();
  test_daily_summary_body_reports_all_five_channels_and_states();
  std::cout << "daily summary tests passed\n";
}
