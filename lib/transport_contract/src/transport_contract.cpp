#include "transport_contract.hpp"

#include <cstdio>
#include <limits>

namespace aquarium::transport {
namespace {

EventType logical_type(EventType type) {
  switch (type) {
    case EventType::high_temperature:
    case EventType::high_temperature_critical:
      return EventType::high_temperature;
    case EventType::low_temperature:
    case EventType::low_temperature_critical:
      return EventType::low_temperature;
    default:
      return type;
  }
}

bool important_event(const TemperatureEvent& event) {
  return event.type == EventType::sensor_fault ||
         event.severity == Severity::n3;
}

std::string number(double value) {
  char buffer[24]{};
  std::snprintf(buffer, sizeof(buffer), "%.6g", value);
  return buffer;
}

std::string temperature_text(double value) {
  char buffer[24]{};
  std::snprintf(buffer, sizeof(buffer), "%.1f°C", value);
  return buffer;
}

std::string event_name(const TemperatureEvent& event) {
  switch (event.type) {
    case EventType::high_temperature:
    case EventType::high_temperature_critical:
      return (event.type == EventType::high_temperature_critical ||
              event.severity == Severity::n3)
                 ? "严重高温"
                 : "高温";
    case EventType::low_temperature:
    case EventType::low_temperature_critical:
      return (event.type == EventType::low_temperature_critical ||
              event.severity == Severity::n3)
                 ? "严重低温"
                 : "低温";
    case EventType::sensor_fault:
      return "温度探头故障";
    case EventType::temperature_rapid_change:
      return event.severity == Severity::n3 ? "严重温度变化" : "温度变化过快";
    case EventType::temperature_gradient:
      return "主缸与回水缸温差";
  }
  return "温度";
}

std::string bark_title(const TemperatureEvent& event,
                       const std::string& label) {
  const std::string name = event_name(event);
  const char* severity = event.state == EventState::resolved
                             ? "信息"
                             : event.severity == Severity::n3 ? "严重" : "注意";
  const char* notification = "首次";
  switch (event.state) {
    case EventState::opened:
      notification = event.notification_number > 1U ? "升级" : "首次";
      break;
    case EventState::escalated:
      notification = "升级";
      break;
    case EventState::reminder:
      notification = "重复";
      break;
    case EventState::resolved:
      notification = "恢复";
      break;
  }
  std::string title = "【" + std::string(severity) + "·" + notification +
                      "】" + label + "｜";
  if (event.state == EventState::resolved) {
    title += name + "已恢复";
  } else if (event.state == EventState::reminder) {
    title += name + "未解除";
  } else {
    title += name;
  }
  return title;
}

const char* event_time_label(EventState state) {
  switch (state) {
    case EventState::opened:
      return "告警判定";
    case EventState::escalated:
      return "升级判定";
    case EventState::reminder:
      return "提醒判定";
    case EventState::resolved:
      return "解除判定";
  }
  return "事件判定";
}

std::string suggestion(const TemperatureEvent& event) {
  if (event.state == EventState::resolved) return "继续观察水温。";
  switch (event.type) {
    case EventType::high_temperature:
    case EventType::high_temperature_critical:
      return "核查最新水温及加热棒温控。";
    case EventType::low_temperature:
    case EventType::low_temperature_critical:
      return "核查最新水温及加热设备。";
    case EventType::temperature_rapid_change:
      return "核查最新水温及探头位置。";
    case EventType::temperature_gradient:
      return "核查水流循环及探头位置。";
    case EventType::sensor_fault:
      return event.state == EventState::resolved ? "继续观察水温。"
                                                  : "检查探头、接线和防水接头。";
  }
  return "检查鱼缸监控设备。";
}

std::string calendar_time_text(std::optional<std::time_t> value,
                               const char* missing_text) {
  if (!value.has_value()) {
    return missing_text;
  }
  if (*value < 0) {
    return "时间不可用";
  }
  constexpr std::time_t kShanghaiOffsetSeconds = 8 * 60 * 60;
  if (*value > std::numeric_limits<std::time_t>::max() -
                  kShanghaiOffsetSeconds ||
      *value < std::numeric_limits<std::time_t>::min() +
                  kShanghaiOffsetSeconds) {
    return "时间不可用";
  }
  const std::time_t local_epoch = *value + kShanghaiOffsetSeconds;
  std::tm broken_down{};
  if (gmtime_r(&local_epoch, &broken_down) == nullptr) {
    return "时间不可用";
  }
  char buffer[32]{};
  if (std::snprintf(buffer, sizeof(buffer), "%04d-%02d-%02d %02d:%02d:%02d",
                    broken_down.tm_year + 1900, broken_down.tm_mon + 1,
                    broken_down.tm_mday, broken_down.tm_hour,
                    broken_down.tm_min, broken_down.tm_sec) < 0) {
    return "时间不可用";
  }
  return buffer;
}

std::string bark_body(const TemperatureEvent& event,
                      std::optional<std::time_t> event_time,
                      std::optional<std::time_t> send_time,
                      const char* source) {
  const bool sensor_fault_without_reading =
      event.type == EventType::sensor_fault && event.state != EventState::resolved;
  const std::string reading =
      event.type == EventType::temperature_gradient
          ? ("当时主缸水温：" + temperature_text(event.display_c))
          : ("当时水温：" +
             (sensor_fault_without_reading ? "无有效读数"
                                           : temperature_text(event.display_c)));
  const std::string source_text =
      source != nullptr && *source != '\0' ? source : "设备";
  const std::uint32_t notification_number =
      event.notification_number == 0U ? 1U : event.notification_number;
  std::string count_text;
  if (event.state == EventState::resolved) {
    count_text = "不计入告警次数";
  } else {
    count_text = "本问题第 " + std::to_string(notification_number) +
                 " 次告警";
    if (event.state == EventState::reminder) {
      const auto repeat_number = event.repeat_number == 0U
                                     ? 1U
                                     : event.repeat_number;
      count_text += "；重复 " + std::to_string(repeat_number) + "/2";
    } else if (!important_event(event)) {
      count_text += "；本问题不重复提醒";
    }
  }
  std::string body = "设备：" + source_text + "\n" +
                     "情况：" + reading + "\n" +
                     "建议：" + suggestion(event) + "\n" +
                     "时间：" + event_time_label(event.state) + "：" +
                     calendar_time_text(event_time, "时间未同步") +
                     "；发送：" +
                     calendar_time_text(send_time, "时间不可用") +
                     "（北京时间）\n" +
                     "次数：" + count_text;
  if (event.state == EventState::reminder) {
    body += "\n截至本次判定，异常仍未解除。";
  }
  if (event.type == EventType::sensor_fault &&
      event.state == EventState::resolved) {
    body += "\n探头读数已恢复。";
  }
  if (event_time.has_value() && send_time.has_value() &&
      *send_time < *event_time) {
    body += "\n设备时钟已调整，不能用上述时间差判断延迟。";
  }
  return body;
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

std::optional<TemperatureEvent> BarkAlertPolicy::prepare(
    const TemperatureEvent& event, const std::string& scope) {
  const auto type = logical_type(event.type);
  Slot* slot = nullptr;
  for (auto& candidate : slots_) {
    if (candidate.used && candidate.scope == scope && candidate.type == type) {
      slot = &candidate;
      break;
    }
  }
  if (slot == nullptr) {
    for (auto& candidate : slots_) {
      if (!candidate.used) {
        candidate.used = true;
        candidate.scope = scope;
        candidate.type = type;
        slot = &candidate;
        break;
      }
    }
  }
  if (slot == nullptr) return std::nullopt;

  if (event.state == EventState::resolved) {
    if (!slot->active) return std::nullopt;
    auto resolved = event;
    resolved.notification_number = 0;
    resolved.repeat_number = 0;
    *slot = {};
    return resolved;
  }

  if (event.state == EventState::reminder) {
    if (!slot->active || !important_event(event) ||
        slot->repeat_number >= 2U) {
      return std::nullopt;
    }
    auto reminder = event;
    ++slot->repeat_number;
    ++slot->notification_number;
    reminder.notification_number = slot->notification_number;
    reminder.repeat_number = slot->repeat_number;
    return reminder;
  }

  if (slot->active) {
    if (!slot->severe && important_event(event)) {
      slot->severe = true;
      ++slot->notification_number;
      auto escalated = event;
      escalated.state = EventState::escalated;
      escalated.notification_number = slot->notification_number;
      escalated.repeat_number = 0;
      return escalated;
    }
    return std::nullopt;
  }

  slot->active = true;
  slot->severe = important_event(event);
  slot->notification_number = 1U;
  slot->repeat_number = 0U;
  auto first = event;
  first.notification_number = 1U;
  first.repeat_number = 0U;
  return first;
}

EventType BarkAlertPolicy::problem_type(EventType type) {
  return logical_type(type);
}

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
                         const std::string& aquarium_id,
                         std::optional<std::time_t> event_time,
                         std::optional<std::time_t> send_time,
                         const char* source) {
  auto message = bark_message(
      ScopedTemperatureEvent{"main_tank", "包包缸·主缸", event, event_time},
      aquarium_id, event_time, send_time, source);
  message.fingerprint = "aquarium:" + aquarium_id + ":" + event_type_name(event.type);
  return message;
}

BarkMessage bark_message(const ScopedTemperatureEvent& scoped_event,
                         const std::string& aquarium_id,
                         std::optional<std::time_t> event_time,
                         std::optional<std::time_t> send_time,
                         const char* source) {
  const auto& event = scoped_event.event;
  const bool critical = event.severity == Severity::n3;
  const std::string event_name = event_type_name(event.type);
  const std::string label = scoped_event.tank_label.empty()
                                ? scoped_event.tank_key
                                : scoped_event.tank_label;
  const auto effective_event_time =
      event_time.has_value() ? event_time : scoped_event.event_time;
  return BarkMessage{bark_title(event, label),
                     bark_body(event, effective_event_time, send_time, source),
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
