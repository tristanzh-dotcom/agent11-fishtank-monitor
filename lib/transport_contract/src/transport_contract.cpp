#include "transport_contract.hpp"

#include <cstdio>

namespace aquarium::transport {
namespace {

std::string number(double value) {
  char buffer[24]{};
  std::snprintf(buffer, sizeof(buffer), "%.6g", value);
  return buffer;
}

std::string json_escape(const std::string& value) {
  std::string escaped;
  for (const unsigned char character : value) {
    switch (character) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (character < 0x20U) {
          char encoded[7]{};
          std::snprintf(encoded, sizeof(encoded), "\\u%04x", character);
          escaped += encoded;
        } else {
          escaped += static_cast<char>(character);
        }
    }
  }
  return escaped;
}

}  // namespace

const char* event_type_name(EventType type) {
  switch (type) {
    case EventType::high_temperature:
      return "high_temperature";
    case EventType::high_temperature_critical:
      return "high_temperature_critical";
    case EventType::low_temperature:
      return "low_temperature";
    case EventType::low_temperature_critical:
      return "low_temperature_critical";
    case EventType::sensor_fault:
      return "sensor_fault";
    case EventType::temperature_rapid_change:
      return "temperature_rapid_change";
    case EventType::temperature_gradient:
      return "temperature_gradient";
  }
  return "unknown";
}

const char* event_state_name(EventState state) {
  switch (state) {
    case EventState::opened:
      return "opened";
    case EventState::escalated:
      return "escalated";
    case EventState::reminder:
      return "reminder";
    case EventState::resolved:
      return "resolved";
  }
  return "unknown";
}

const char* severity_name(Severity severity) {
  return severity == Severity::n3 ? "N3" : "N2";
}

std::string telemetry_json(const TemperatureSample& sample) {
  std::string payload = "{\"timestamp_ms\":" + std::to_string(sample.at_ms);
  if (sample.display_c.has_value()) {
    payload += ",\"display_c\":" + number(*sample.display_c);
  }
  if (sample.return_c.has_value()) {
    payload += ",\"return_c\":" + number(*sample.return_c);
  }
  return payload + "}";
}

std::string event_json(const TemperatureEvent& event) {
  const std::string event_name = event_type_name(event.type);
  const std::string state_name = event_state_name(event.state);
  const std::string event_id = event_name + ":" + state_name + ":" +
                               std::to_string(event.at_ms);
  return "{\"event_type\":\"" + event_name +
         "\",\"state\":\"" + event_state_name(event.state) +
         "\",\"severity\":\"" + severity_name(event.severity) +
         "\",\"timestamp_ms\":" + std::to_string(event.at_ms) +
         ",\"display_c\":" + number(event.display_c) +
         ",\"event_id\":\"" + event_id + "\"}";
}

BarkMessage bark_message(const TemperatureEvent& event,
                         const std::string& aquarium_id) {
  const bool critical = event.severity == Severity::n3;
  const std::string event_name = event_type_name(event.type);
  return BarkMessage{critical ? "鱼缸温度严重告警" : "鱼缸温度告警",
                     "事件=" + event_name + " 状态=" +
                         event_state_name(event.state) + " 水温=" +
                         number(event.display_c) + "C",
                     "aquarium", critical ? "critical" : "timeSensitive",
                     "aquarium:" + aquarium_id + ":" + event_name};
}

BarkMessage bark_message(const ScopedTemperatureEvent& scoped_event,
                         const std::string& aquarium_id) {
  const auto& event = scoped_event.event;
  const bool critical = event.severity == Severity::n3;
  const std::string event_name = event_type_name(event.type);
  const std::string label = scoped_event.tank_label.empty()
                                ? scoped_event.tank_key
                                : scoped_event.tank_label;
  const std::string body = event.type == EventType::sensor_fault
                               ? label + " 事件=" + event_name + " 状态=" +
                                     event_state_name(event.state) +
                                     " 无有效读数"
                               : label + " 事件=" + event_name + " 状态=" +
                                     event_state_name(event.state) + " 水温=" +
                                     number(event.display_c) + "C";
  return BarkMessage{critical ? label + "温度严重告警" : label + "温度告警",
                     body,
                     "aquarium",
                     critical ? "critical" : "timeSensitive",
                     "aquarium:" + aquarium_id + ":" + scoped_event.tank_key +
                         ":" + event_name};
}

std::string bark_request_json(const BarkMessage& message,
                              const std::string& device_key) {
  return "{\"device_key\":\"" + json_escape(device_key) +
         "\",\"title\":\"" + json_escape(message.title) +
         "\",\"body\":\"" + json_escape(message.body) +
         "\",\"group\":\"" + json_escape(message.group) +
         "\",\"level\":\"" + json_escape(message.level) + "\"}";
}

}  // namespace aquarium::transport
