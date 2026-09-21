#include "temperature_engine.hpp"

#include <cassert>
#include <iostream>

using aquarium::EventState;
using aquarium::EventType;
using aquarium::Severity;
using aquarium::TemperatureEngine;
using aquarium::TemperatureSample;

namespace {

TemperatureSample sample(std::uint64_t at_ms, double display) {
  return TemperatureSample{at_ms, display, display};
}

void test_high_temperature_opens_only_after_15_minutes() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 27.6)).empty());
  assert(engine.ingest(sample(899999, 27.6)).empty());

  const auto events = engine.ingest(sample(900000, 27.6));
  assert(events.size() == 1);
  assert(events[0].type == EventType::high_temperature);
  assert(events[0].state == EventState::opened);
}

void test_high_temperature_escalates_after_5_minutes() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 28.6)).empty());

  const auto events = engine.ingest(sample(300000, 28.6));
  assert(events.size() == 1);
  assert(events[0].type == EventType::high_temperature_critical);
  assert(events[0].state == EventState::opened);
}

void test_high_temperature_resolves_at_attention_threshold_after_15_minutes() {
  TemperatureEngine engine;

  engine.ingest(sample(0, 27.6));
  const auto opened = engine.ingest(sample(900000, 27.6));
  assert(opened.size() == 1);

  assert(engine.ingest(sample(900001, 27.5)).empty());
  assert(engine.ingest(sample(1800000, 27.5)).empty());

  const auto resolved = engine.ingest(sample(1800001, 27.5));
  assert(resolved.size() == 1);
  assert(resolved[0].type == EventType::high_temperature);
  assert(resolved[0].state == EventState::resolved);
}

void test_low_temperature_resolves_at_attention_threshold_after_15_minutes() {
  TemperatureEngine engine;

  engine.ingest(sample(0, 23.4));
  const auto opened = engine.ingest(sample(900000, 23.4));
  assert(opened.size() == 1);
  assert(opened[0].type == EventType::low_temperature);

  assert(engine.ingest(sample(900001, 23.5)).empty());
  assert(engine.ingest(sample(1800000, 23.5)).empty());

  const auto resolved = engine.ingest(sample(1800001, 23.5));
  assert(resolved.size() == 1);
  assert(resolved[0].type == EventType::low_temperature);
  assert(resolved[0].state == EventState::resolved);
}

void test_two_invalid_display_samples_open_a_sensor_fault() {
  TemperatureEngine engine;

  assert(engine.ingest(TemperatureSample{0, std::nullopt, 26.0}).empty());
  const auto events =
      engine.ingest(TemperatureSample{30000, std::nullopt, 26.0});

  assert(events.size() == 1);
  assert(events[0].type == EventType::sensor_fault);
  assert(events[0].state == EventState::opened);
  assert(events[0].severity == Severity::n2);
}

void test_one_degree_change_over_30_minutes_opens_rapid_change() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 26.0)).empty());
  const auto events = engine.ingest(sample(1800000, 27.0));

  assert(events.size() == 1);
  assert(events[0].type == EventType::temperature_rapid_change);
  assert(events[0].state == EventState::opened);
}

void test_sustained_display_return_gradient_opens_diagnostic() {
  TemperatureEngine engine;

  assert(engine.ingest(TemperatureSample{0, 26.0, 25.1}).empty());
  assert(engine.ingest(TemperatureSample{599999, 26.0, 25.1}).empty());
  const auto events =
      engine.ingest(TemperatureSample{600000, 26.0, 25.1});

  assert(events.size() == 1);
  assert(events[0].type == EventType::temperature_gradient);
  assert(events[0].state == EventState::opened);
}

void test_low_temperature_opens_only_after_15_minutes() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 23.4)).empty());
  assert(engine.ingest(sample(899999, 23.4)).empty());
  const auto events = engine.ingest(sample(900000, 23.4));

  assert(events.size() == 1);
  assert(events[0].type == EventType::low_temperature);
  assert(events[0].state == EventState::opened);
}

void test_low_temperature_critical_opens_after_5_minutes() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 22.4)).empty());
  const auto events = engine.ingest(sample(300000, 22.4));

  assert(events.size() == 1);
  assert(events[0].type == EventType::low_temperature_critical);
  assert(events[0].state == EventState::opened);
}

void test_two_degree_change_over_30_minutes_opens_critical_rapid_change() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 26.0)).empty());
  const auto events = engine.ingest(sample(1800000, 28.0));

  assert(events.size() == 1);
  assert(events[0].type == EventType::temperature_rapid_change);
  assert(events[0].state == EventState::opened);
  assert(events[0].severity == Severity::n3);
}

void test_rapid_change_resolves_after_30_minutes_of_stability_and_reopens() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 26.0)).empty());
  const auto opened = engine.ingest(sample(1800000, 27.0));
  assert(opened.size() == 1);
  assert(opened[0].type == EventType::temperature_rapid_change);
  assert(opened[0].state == EventState::opened);
  assert(opened[0].severity == Severity::n2);

  assert(engine.ingest(sample(3600000, 27.0)).empty());
  const auto resolved = engine.ingest(sample(5400000, 27.0));
  assert(resolved.size() == 1);
  assert(resolved[0].type == EventType::temperature_rapid_change);
  assert(resolved[0].state == EventState::resolved);
  assert(resolved[0].severity == Severity::n2);

  const auto reopened = engine.ingest(sample(7200000, 28.0));
  assert(reopened.size() == 1);
  assert(reopened[0].type == EventType::temperature_rapid_change);
  assert(reopened[0].state == EventState::opened);
}

void test_rapid_change_escalates_once_and_resolves_at_peak_severity() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 24.0)).empty());
  assert(engine.ingest(sample(900000, 24.0)).empty());
  const auto opened = engine.ingest(sample(1800000, 25.0));
  assert(opened.size() == 1);
  assert(opened[0].state == EventState::opened);
  assert(opened[0].severity == Severity::n2);

  const auto escalated = engine.ingest(sample(2700000, 26.0));
  assert(escalated.size() == 1);
  assert(escalated[0].type == EventType::temperature_rapid_change);
  assert(escalated[0].state == EventState::escalated);
  assert(escalated[0].severity == Severity::n3);

  assert(engine.ingest(sample(3600000, 26.0)).empty());
  assert(engine.ingest(sample(4500000, 26.0)).empty());
  const auto resolved = engine.ingest(sample(6300000, 26.0));
  assert(resolved.size() == 1);
  assert(resolved[0].type == EventType::temperature_rapid_change);
  assert(resolved[0].state == EventState::resolved);
  assert(resolved[0].severity == Severity::n3);
}

void test_gradient_requires_hysteresis_and_fresh_recovery_duration() {
  TemperatureEngine engine;

  assert(engine.ingest(TemperatureSample{0, 26.0, 25.1}).empty());
  const auto opened = engine.ingest(TemperatureSample{600000, 26.0, 25.1});
  assert(opened.size() == 1);
  assert(opened[0].type == EventType::temperature_gradient);
  assert(opened[0].state == EventState::opened);

  assert(engine.ingest(TemperatureSample{600001, 26.0, 25.2}).empty());
  assert(engine.ingest(TemperatureSample{900000, 26.0, 25.3}).empty());
  assert(engine.ingest(TemperatureSample{900001, 26.0, 25.2}).empty());
  const auto resolved =
      engine.ingest(TemperatureSample{1500001, 26.0, 25.2});
  assert(resolved.size() == 1);
  assert(resolved[0].type == EventType::temperature_gradient);
  assert(resolved[0].state == EventState::resolved);
}

void test_sensor_fault_suppresses_temperature_reminders_and_requires_fresh_recovery() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 27.6)).empty());
  const auto high_opened = engine.ingest(sample(900000, 27.6));
  assert(high_opened.size() == 1);
  assert(high_opened[0].type == EventType::high_temperature);
  assert(high_opened[0].state == EventState::opened);

  assert(engine.ingest(TemperatureSample{930000, std::nullopt, 26.0}).empty());
  const auto fault_opened =
      engine.ingest(TemperatureSample{960000, std::nullopt, 26.0});
  assert(fault_opened.size() == 1);
  assert(fault_opened[0].type == EventType::sensor_fault);
  assert(fault_opened[0].state == EventState::opened);

  const auto fault_reminder =
      engine.ingest(TemperatureSample{4560000, std::nullopt, 26.0});
  assert(fault_reminder.size() == 1);
  assert(fault_reminder[0].type == EventType::sensor_fault);
  assert(fault_reminder[0].state == EventState::reminder);

  const auto fault_resolved = engine.ingest(sample(4590000, 26.5));
  assert(fault_resolved.size() == 1);
  assert(fault_resolved[0].type == EventType::sensor_fault);
  assert(fault_resolved[0].state == EventState::resolved);

  const auto high_resolved = engine.ingest(sample(5490000, 27.5));
  assert(high_resolved.size() == 1);
  assert(high_resolved[0].type == EventType::high_temperature);
  assert(high_resolved[0].state == EventState::resolved);
}

void test_critical_high_temperature_reminds_after_60_minutes() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 28.6)).empty());
  const auto opened = engine.ingest(sample(300000, 28.6));
  assert(opened.size() == 1);
  assert(opened[0].type == EventType::high_temperature_critical);
  assert(opened[0].state == EventState::opened);

  const auto reminder = engine.ingest(sample(3900000, 28.6));
  assert(reminder.size() == 1);
  assert(reminder[0].type == EventType::high_temperature_critical);
  assert(reminder[0].state == EventState::reminder);
}

void test_critical_low_temperature_reminds_after_60_minutes() {
  TemperatureEngine engine;

  assert(engine.ingest(sample(0, 22.4)).empty());
  const auto opened = engine.ingest(sample(300000, 22.4));
  assert(opened.size() == 1);
  assert(opened[0].type == EventType::low_temperature_critical);

  const auto reminder = engine.ingest(sample(3900000, 22.4));
  assert(reminder.size() == 1);
  assert(reminder[0].type == EventType::low_temperature_critical);
  assert(reminder[0].state == EventState::reminder);
}

void test_auxiliary_engines_are_independent_without_gradient_events() {
  aquarium::TemperaturePolicy old_four_policy{};
  old_four_policy.low_attention_c = 22.5;
  old_four_policy.low_critical_c = 20.5;
  old_four_policy.low_recovery_c = 22.5;

  aquarium::TemperaturePolicy normal_policy{};
  TemperatureEngine old_four(old_four_policy);
  TemperatureEngine xiaohei(normal_policy);
  TemperatureEngine maomao(normal_policy);

  assert(old_four.ingest(TemperatureSample{0, 20.0, std::nullopt}).empty());
  assert(xiaohei.ingest(TemperatureSample{0, 25.0, std::nullopt}).empty());
  assert(maomao.ingest(TemperatureSample{0, 25.0, std::nullopt}).empty());

  const auto old_four_events =
      old_four.ingest(TemperatureSample{300000, 20.0, std::nullopt});
  assert(old_four_events.size() == 1);
  assert(old_four_events[0].type == EventType::low_temperature_critical);
  assert(old_four_events[0].severity == Severity::n3);
  assert(xiaohei.ingest(TemperatureSample{300000, 25.0, std::nullopt}).empty());
  assert(maomao.ingest(TemperatureSample{300000, 25.0, std::nullopt}).empty());
}

}  // namespace

int main() {
  test_high_temperature_opens_only_after_15_minutes();
  test_high_temperature_escalates_after_5_minutes();
  test_high_temperature_resolves_at_attention_threshold_after_15_minutes();
  test_low_temperature_resolves_at_attention_threshold_after_15_minutes();
  test_two_invalid_display_samples_open_a_sensor_fault();
  test_one_degree_change_over_30_minutes_opens_rapid_change();
  test_sustained_display_return_gradient_opens_diagnostic();
  test_low_temperature_opens_only_after_15_minutes();
  test_low_temperature_critical_opens_after_5_minutes();
  test_two_degree_change_over_30_minutes_opens_critical_rapid_change();
  test_rapid_change_resolves_after_30_minutes_of_stability_and_reopens();
  test_rapid_change_escalates_once_and_resolves_at_peak_severity();
  test_gradient_requires_hysteresis_and_fresh_recovery_duration();
  test_sensor_fault_suppresses_temperature_reminders_and_requires_fresh_recovery();
  test_critical_high_temperature_reminds_after_60_minutes();
  test_critical_low_temperature_reminds_after_60_minutes();
  test_auxiliary_engines_are_independent_without_gradient_events();
  std::cout << "temperature engine task-1 tests passed\n";
}
