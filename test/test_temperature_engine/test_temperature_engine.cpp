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

void test_high_temperature_resolves_only_after_hysteresis_duration() {
  TemperatureEngine engine;

  engine.ingest(sample(0, 27.6));
  const auto opened = engine.ingest(sample(900000, 27.6));
  assert(opened.size() == 1);

  assert(engine.ingest(sample(900001, 26.5)).empty());
  const auto still_recovering = engine.ingest(sample(1800000, 26.5));
  assert(still_recovering.size() == 1);
  assert(still_recovering[0].type == EventType::temperature_rapid_change);

  const auto resolved = engine.ingest(sample(1800001, 26.5));
  assert(resolved.size() == 1);
  assert(resolved[0].type == EventType::high_temperature);
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
  assert(events[0].type == EventType::temperature_rapid_change_critical);
  assert(events[0].severity == Severity::n3);
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

}  // namespace

int main() {
  test_high_temperature_opens_only_after_15_minutes();
  test_high_temperature_escalates_after_5_minutes();
  test_high_temperature_resolves_only_after_hysteresis_duration();
  test_two_invalid_display_samples_open_a_sensor_fault();
  test_one_degree_change_over_30_minutes_opens_rapid_change();
  test_sustained_display_return_gradient_opens_diagnostic();
  test_low_temperature_opens_only_after_15_minutes();
  test_low_temperature_critical_opens_after_5_minutes();
  test_two_degree_change_over_30_minutes_opens_critical_rapid_change();
  test_critical_high_temperature_reminds_after_60_minutes();
  test_critical_low_temperature_reminds_after_60_minutes();
  std::cout << "temperature engine task-1 tests passed\n";
}
