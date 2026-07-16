#pragma once

#include "temperature_engine.hpp"

#include <string>

namespace aquarium::transport {

struct BarkMessage {
  std::string title;
  std::string body;
  std::string group;
  std::string level;
  std::string fingerprint;
};

const char* event_type_name(EventType type);
const char* event_state_name(EventState state);
const char* severity_name(Severity severity);
std::string telemetry_json(const TemperatureSample& sample);
std::string event_json(const TemperatureEvent& event);
BarkMessage bark_message(const TemperatureEvent& event,
                         const std::string& aquarium_id);
std::string bark_request_json(const BarkMessage& message,
                              const std::string& device_key);

}  // namespace aquarium::transport
