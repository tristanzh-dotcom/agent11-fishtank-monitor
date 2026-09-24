#include "daily_summary.hpp"
#include "heartbeat_contract.hpp"

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
  value.main_c = 22.1;
  value.sump_c = 21.8;
  value.auxiliary_c[0] = 22.1;
  value.auxiliary_c[1] = 28.7;
  value.auxiliary_c[2] = 20.1;
  value.auxiliary_states[0] = SummaryReadingState::valid;
  value.auxiliary_states[1] = SummaryReadingState::valid;
  value.auxiliary_states[2] = SummaryReadingState::valid;
  value.main_status = aquarium::transport::TemperatureReadingStatus::low;
  value.auxiliary_status[0] = aquarium::transport::TemperatureReadingStatus::low;
  value.auxiliary_status[1] =
      aquarium::transport::TemperatureReadingStatus::high_critical;
  value.auxiliary_status[2] =
      aquarium::transport::TemperatureReadingStatus::low_critical;
  // The grass reading is independently received from ESP3.
  value.extension_tanks[0].state = SummaryReadingState::valid;
  value.extension_tanks[0].temperature_c = 25.4;
  value.extension_tanks[1].state = SummaryReadingState::valid;
  value.extension_tanks[1].temperature_c = 27.1;
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
  assert(body.size() <= aquarium::heartbeat::kMaxTemperatureSummaryBytes);
  assert(body.find("采样时间：2026-09-09 09:00:20") != std::string::npos);
  assert(body.find("包包缸（主缸 / 回水缸）") != std::string::npos);
  assert(body.find("  主缸：22.1°C（温度偏低）") != std::string::npos);
  assert(body.find("  回水缸：21.8°C") != std::string::npos);
  assert(body.find("老四缸：22.1°C（温度偏低）") != std::string::npos);
  assert(body.find("小黑缸：28.7°C（温度严重偏高）") != std::string::npos);
  assert(body.find("毛毛缸：20.1°C（温度严重偏低）") != std::string::npos);

  const auto message =
      aquarium::transport::daily_summary_message(*created, "esp1");
  assert(message.title == "【信息·汇总】全部鱼缸｜鱼缸温度报告");
  assert(message.body.find("设备：温控ESP1号") != std::string::npos);
  assert(message.body.find("建议：无需操作") != std::string::npos);
  assert(message.body.find("提醒次数：") == std::string::npos);
  assert(message.group == "aquarium-daily");
  assert(message.level == "active");
  assert(message.fingerprint == "aquarium:esp1:20260909:morning");
}

void test_temperature_status_uses_configured_thresholds() {
  aquarium::TemperaturePolicy policy{};
  assert(aquarium::transport::temperature_reading_status(23.5, policy) ==
         aquarium::transport::TemperatureReadingStatus::normal);
  assert(aquarium::transport::temperature_reading_status(23.49, policy) ==
         aquarium::transport::TemperatureReadingStatus::low);
  assert(aquarium::transport::temperature_reading_status(22.49, policy) ==
         aquarium::transport::TemperatureReadingStatus::low_critical);
  assert(aquarium::transport::temperature_reading_status(27.5, policy) ==
         aquarium::transport::TemperatureReadingStatus::normal);
  assert(aquarium::transport::temperature_reading_status(27.51, policy) ==
         aquarium::transport::TemperatureReadingStatus::high);
  assert(aquarium::transport::temperature_reading_status(28.51, policy) ==
         aquarium::transport::TemperatureReadingStatus::high_critical);

  aquarium::TemperaturePolicy laosi_policy{};
  laosi_policy.low_attention_c = 22.5;
  laosi_policy.low_critical_c = 20.5;
  assert(aquarium::transport::temperature_reading_status(22.1, laosi_policy) ==
         aquarium::transport::TemperatureReadingStatus::low);
  assert(aquarium::transport::temperature_reading_status(20.1, laosi_policy) ==
         aquarium::transport::TemperatureReadingStatus::low_critical);
}

void test_daily_summary_body_appends_new_tanks_without_relabeling_old_ones() {
  DailySummaryScheduler scheduler;
  const auto created = scheduler.observe(
      LocalDateTime{2026, 9, 9, 9, 0, 20, true}, 1000, snapshot());
  assert(created.has_value());
  const auto body = aquarium::transport::daily_summary_body(*created);
  assert(body.find("南美草缸：25.4°C") != std::string::npos);
  assert(body.find("南美异形缸：27.1°C") != std::string::npos);
  assert(body.find("毛毛缸：20.1°C（温度严重偏低）") != std::string::npos);
  assert(body.size() <= aquarium::heartbeat::kMaxTemperatureSummaryBytes);
}

void test_daily_summary_marks_retained_extension_value_as_not_updated() {
  DailySummaryScheduler scheduler;
  auto value = snapshot();
  value.extension_tanks[1].fresh = false;
  const auto created = scheduler.observe(
      LocalDateTime{2026, 9, 9, 9, 0, 20, true}, 1000, value);
  assert(created.has_value());
  const auto body = aquarium::transport::daily_summary_body(*created);
  assert(body.find("南美异形缸：27.1°C（数据暂未更新）") !=
         std::string::npos);
  assert(body.find("南美异形缸：无有效读数") == std::string::npos);
}

}  // namespace

int main() {
  test_daily_summary_windows_and_expiry();
  test_daily_summary_rejects_invalid_time_and_handles_slot_order();
  test_daily_summary_body_reports_all_five_channels_and_states();
  test_temperature_status_uses_configured_thresholds();
  test_daily_summary_body_appends_new_tanks_without_relabeling_old_ones();
  test_daily_summary_marks_retained_extension_value_as_not_updated();
  std::cout << "daily summary tests passed\n";
}
