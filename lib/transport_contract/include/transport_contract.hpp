#pragma once

#include "temperature_engine.hpp"

#include <array>
#include <cstddef>
#include <ctime>
#include <cstdint>
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

enum class ExtensionConnectivityEvent : std::uint8_t { offline, recovered };

struct ScopedTemperatureEvent {
  std::string tank_key;
  std::string tank_label;
  TemperatureEvent event;
  std::optional<std::time_t> event_time = std::nullopt;
};

class BarkAlertPolicy {
 public:
  static EventType problem_type(EventType type);
  std::optional<TemperatureEvent> prepare(const TemperatureEvent& event,
                                          const std::string& scope);

 private:
  struct Slot {
    bool used = false;
    std::string scope;
    EventType type = EventType::high_temperature;
    bool active = false;
    bool severe = false;
    std::uint8_t notification_number = 0;
    std::uint8_t repeat_number = 0;
  };

  static constexpr std::size_t kSlotCount = 32U;
  std::array<Slot, kSlotCount> slots_{};
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
BarkMessage extension_connectivity_message(
    ExtensionConnectivityEvent event, std::uint64_t stale_after_ms,
    std::optional<std::time_t> event_time = std::nullopt,
    std::optional<std::time_t> send_time = std::nullopt);
std::string bark_request_json(const BarkMessage& message,
                              const std::string& device_key);

}  // namespace aquarium::transport
