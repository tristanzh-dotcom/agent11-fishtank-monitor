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

std::uint64_t saturating_add(std::uint64_t left, std::uint64_t right) {
  if (right > std::numeric_limits<std::uint64_t>::max() - left) {
    return std::numeric_limits<std::uint64_t>::max();
  }
  return left + right;
}

}  // namespace

std::string heartbeat_json(const HeartbeatPayload& payload) {
  std::ostringstream stream;
  stream.imbue(std::locale::classic());
  stream << "{\"device_id\":\"" << json_escape(payload.device_id)
         << "\",\"sent_at_ms\":" << payload.sent_at_ms << ",\"nonce\":\""
         << json_escape(payload.nonce) << "\",\"main_c\":"
         << json_number(payload.main_c) << ",\"sump_c\":"
         << json_number(payload.sump_c) << ",\"uptime_ms\":"
         << payload.uptime_ms << '}';
  return stream.str();
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

