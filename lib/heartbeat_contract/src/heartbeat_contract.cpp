#include "heartbeat_contract.hpp"

#include <algorithm>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>

namespace aquarium::heartbeat {
namespace {

std::string json_escape(const std::string& value) {
  std::ostringstream escaped;
  escaped << std::hex << std::setfill('0');
  for (const unsigned char character : value) {
    switch (character) {
      case '"':
        escaped << "\\\"";
        break;
      case '\\':
        escaped << "\\\\";
        break;
      case '\b':
        escaped << "\\b";
        break;
      case '\f':
        escaped << "\\f";
        break;
      case '\n':
        escaped << "\\n";
        break;
      case '\r':
        escaped << "\\r";
        break;
      case '\t':
        escaped << "\\t";
        break;
      default:
        if (character < 0x20U) {
          escaped << "\\u" << std::setw(4)
                  << static_cast<unsigned int>(character);
        } else {
          escaped << static_cast<char>(character);
        }
    }
  }
  return escaped.str();
}

std::string json_number(double value) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << std::fixed << std::setprecision(2) << value;
  auto text = stream.str();
  while (!text.empty() && text.back() == '0') {
    text.pop_back();
  }
  if (!text.empty() && text.back() == '.') {
    text.pop_back();
  }
  if (text == "-0") {
    return "0";
  }
  return text;
}

std::string json_optional_number(const std::optional<double>& value) {
  return value.has_value() ? json_number(*value) : "null";
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
  return severity == Severity::n3 ? "n3" : "n2";
}

std::uint64_t saturating_add(std::uint64_t left, std::uint64_t right) {
  if (right > std::numeric_limits<std::uint64_t>::max() - left) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return left + right;
}

bool valid_utf8(const std::string& value) {
  std::size_t index = 0;
  while (index < value.size()) {
    const auto lead = static_cast<unsigned char>(value[index]);
    if (lead <= 0x7fU) {
      ++index;
      continue;
    }
    std::size_t length = 0;
    std::uint32_t code_point = 0;
    std::uint32_t minimum = 0;
    std::uint32_t maximum = 0;
    if (lead >= 0xc2U && lead <= 0xdfU) {
      length = 2U;
      code_point = lead & 0x1fU;
      minimum = 0x80U;
      maximum = 0x7ffU;
    } else if (lead >= 0xe0U && lead <= 0xefU) {
      length = 3U;
      code_point = lead & 0x0fU;
      minimum = 0x800U;
      maximum = 0xffffU;
    } else if (lead >= 0xf0U && lead <= 0xf4U) {
      length = 4U;
      code_point = lead & 0x07U;
      minimum = 0x10000U;
      maximum = 0x10ffffU;
    } else {
      return false;
    }
    if (index + length > value.size()) return false;
    for (std::size_t offset = 1U; offset < length; ++offset) {
      const auto byte = static_cast<unsigned char>(value[index + offset]);
      if (byte < 0x80U || byte > 0xbfU) return false;
      code_point = (code_point << 6U) | (byte & 0x3fU);
    }
    if (code_point < minimum || code_point > maximum
        || (code_point >= 0xd800U && code_point <= 0xdfffU)) {
      return false;
    }
    index += length;
  }
  return true;
}

bool valid_temperature_snapshot(const TemperatureSnapshot& snapshot) {
  return snapshot.sampled_at_ms > 0U && !snapshot.summary_text.empty() &&
         valid_utf8(snapshot.summary_text) &&
         snapshot.summary_text.size() <= kMaxTemperatureSummaryBytes;
}

std::string heartbeat_json_impl(const HeartbeatPayload& payload,
                                bool include_temperature_snapshot) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << "{\"device_id\":\"" << json_escape(payload.device_id)
         << "\",\"sent_at_ms\":" << payload.sent_at_ms
         << ",\"nonce\":\"" << json_escape(payload.nonce)
         << "\",\"main_c\":" << json_optional_number(payload.main_c)
         << ",\"sump_c\":" << json_optional_number(payload.sump_c)
         << ",\"uptime_ms\":" << payload.uptime_ms;
  if (include_temperature_snapshot && payload.temperature_snapshot.has_value()) {
    const auto& snapshot = *payload.temperature_snapshot;
    stream << ",\"temperature_snapshot\":{\"sampled_at_ms\":"
           << snapshot.sampled_at_ms << ",\"summary_text\":\""
           << json_escape(snapshot.summary_text) << "\"}";
  }
  stream << ",\"active_events\":[";
  for (std::size_t index = 0; index < payload.active_events.size(); ++index) {
    if (index > 0U) {
      stream << ',';
    }
    const auto& event = payload.active_events[index];
    stream << "{\"type\":\"" << event_type_name(event.type)
           << "\",\"state\":\"" << event_state_name(event.state)
           << "\",\"severity\":\"" << severity_name(event.severity)
           << "\",\"at_ms\":" << event.at_ms << ",\"display_c\":"
           << json_optional_number(event.display_c) << '}';
  }
  stream << "]}";
  return stream.str();
}

}  // namespace

std::string heartbeat_json(const HeartbeatPayload& payload) {
  const bool has_valid_snapshot =
      payload.temperature_snapshot.has_value() &&
      valid_temperature_snapshot(*payload.temperature_snapshot);
  const auto without_snapshot = heartbeat_json_impl(payload, false);
  if (!has_valid_snapshot) {
    return without_snapshot;
  }

  const auto with_snapshot = heartbeat_json_impl(payload, true);
  if (with_snapshot.size() <= kMaxHeartbeatBodyBytes) {
    return with_snapshot;
  }
  return without_snapshot;
}

std::string signature_canonical(
    const HeartbeatPayload& payload,
    const std::string& raw_body_sha256_hex) {
  return "v1\n" + std::to_string(payload.sent_at_ms) + "\n" + payload.nonce +
         "\n" + raw_body_sha256_hex;
}

bool valid_nonce(const std::string& nonce) {
  if (nonce.size() < 16U || nonce.size() > 64U || nonce.size() % 2U != 0U) {
    return false;
  }
  return std::all_of(nonce.begin(), nonce.end(), [](const char character) {
    return (character >= '0' && character <= '9') ||
           (character >= 'a' && character <= 'f');
  });
}

HeartbeatSchedule::HeartbeatSchedule(std::uint64_t nominal_interval_ms,
                                     std::uint64_t initial_retry_ms,
                                     std::uint64_t maximum_retry_ms)
    : nominal_interval_ms_(nominal_interval_ms),
      initial_retry_ms_(initial_retry_ms),
      maximum_retry_ms_(maximum_retry_ms) {}

bool HeartbeatSchedule::should_attempt(std::uint64_t now_ms) const {
  return now_ms >= next_attempt_at_ms_;
}

void HeartbeatSchedule::record_failure(std::uint64_t now_ms) {
  if (current_retry_ms_ == 0U) {
    current_retry_ms_ = initial_retry_ms_;
  } else if (current_retry_ms_ >= maximum_retry_ms_ / 2U) {
    current_retry_ms_ = maximum_retry_ms_;
  } else {
    current_retry_ms_ =
        std::min(current_retry_ms_ * 2U, maximum_retry_ms_);
  }
  next_attempt_at_ms_ = saturating_add(now_ms, current_retry_ms_);
}

void HeartbeatSchedule::record_success(std::uint64_t now_ms) {
  current_retry_ms_ = 0U;
  next_attempt_at_ms_ = saturating_add(now_ms, nominal_interval_ms_);
}

}  // namespace aquarium::heartbeat
