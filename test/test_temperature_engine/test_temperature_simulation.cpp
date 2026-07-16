#include "temperature_engine.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>

using aquarium::EventState;
using aquarium::EventType;
using aquarium::TemperatureEngine;
using aquarium::TemperatureSample;

namespace {

void test_summer_drift_between_26_and_27_has_no_event() {
  TemperatureEngine engine;
  for (std::uint64_t index = 0; index < 5760; ++index) {
    const double temperature = 26.2 + static_cast<double>(index % 120) / 200.0;
    const auto events = engine.ingest(
        TemperatureSample{index * 30000, temperature, temperature});
    assert(events.empty());
  }
}

void test_sustained_high_incident_has_one_open_reminder_and_resolution() {
  TemperatureEngine engine;
  int opened = 0;
  int reminders = 0;
  int resolved = 0;

  for (std::uint64_t index = 0; index <= 180; ++index) {
    const auto events =
        engine.ingest(TemperatureSample{index * 30000, 27.6, 27.6});
    for (const auto& event : events) {
      if (event.type != EventType::high_temperature) {
        continue;
      }
      if (event.state == EventState::opened) {
        ++opened;
      } else if (event.state == EventState::reminder) {
        ++reminders;
      } else if (event.state == EventState::resolved) {
        ++resolved;
      }
    }
  }

  for (std::uint64_t index = 181; index <= 211; ++index) {
    const auto events =
        engine.ingest(TemperatureSample{index * 30000, 26.5, 26.5});
    for (const auto& event : events) {
      if (event.type == EventType::high_temperature &&
          event.state == EventState::resolved) {
        ++resolved;
      }
    }
  }

  assert(opened == 1);
  assert(reminders == 1);
  assert(resolved == 1);
}

void test_seeded_10000_sample_noise_trace_has_no_event() {
  TemperatureEngine engine;
  std::uint32_t state = 0xC0FFEEU;
  for (std::uint64_t index = 0; index < 10000; ++index) {
    state = state * 1664525U + 1013904223U;
    const double temperature = 24.8 + static_cast<double>(state % 400U) / 1000.0;
    const auto events = engine.ingest(
        TemperatureSample{index * 30000, temperature, temperature});
    assert(events.empty());
  }
}

}  // namespace

int main() {
  test_summer_drift_between_26_and_27_has_no_event();
  test_sustained_high_incident_has_one_open_reminder_and_resolution();
  test_seeded_10000_sample_noise_trace_has_no_event();
  std::cout << "temperature simulation tests passed\n";
}
