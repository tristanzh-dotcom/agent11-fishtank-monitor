#pragma once

#include "temperature_engine.hpp"

#include <ctime>
#include <optional>
#include <string>

namespace aquarium::transport {

struct BarkMessage {
  std::string title;
  std::string body;
  std::string group;
  std::string level;
  std::string fingerprint;
};

struct ScopedTemperatureEvent {
  std::string tank_key;
  std::string tank_label;
  TemperatureEvent event;
  std::optional<std::time_t> event_time = std::nullopt;
};

const char* event_type_name(EventType type);
const char* event_state_name(EventState state);
const char* severity_name(Severity severity);
std::string telemetry_json(const TemperatureSample& sample);
std::string event_json(const TemperatureEvent& event);
BarkMessage bark_message(const TemperatureEvent& event,
                         const std::string& aquarium_id,
                         std::optional<std::time_t> event_time = std::nullopt,
                         std::optional<std::time_t> send_time = std::nullopt,
                         const char* source = "设备");
BarkMessage bark_message(const ScopedTemperatureEvent& scoped_event,
                         const std::string& aquarium_id,
                         std::optional<std::time_t> event_time = std::nullopt,
                         std::optional<std::time_t> send_time = std::nullopt,
                         const char* source = "设备");
std::string bark_request_json(const BarkMessage& message,
                              const std::string& device_key);

}  // namespace aquarium::transport
