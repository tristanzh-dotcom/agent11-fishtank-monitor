#include "active_event_snapshot.hpp"
#include "bark_notifier.hpp"
#include "config.hpp"
#include "delivery_coordinator.hpp"
#include "ds18b20_reader.hpp"
#include "heartbeat_contract.hpp"
#include "heartbeat_notifier.hpp"
#include "secrets.hpp"
#include "retry_backoff.hpp"
#include "scoped_event_outbox.hpp"
#include "daily_summary.hpp"
#include "extension_lan_state.hpp"
#if !defined(AQUARIUM_DISABLE_TAB5_LAN)
#include "tab5_lan_state.hpp"
#endif

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#if !defined(AQUARIUM_DISABLE_TAB5_LAN)
#include <WiFiUdp.h>
#endif

#include <array>
#include <algorithm>
#include <ctime>
#include <optional>

namespace {

#if !defined(AQUARIUM_DISABLE_TAB5_LAN)
#include "tab5_lan_secret.hpp"
#endif

constexpr std::uint8_t kOneWirePin = 4;
constexpr std::time_t kMinimumReasonableEpochSeconds = 1700000000;

aquarium::firmware::RuntimeConfig runtime_config =
    aquarium::firmware::default_runtime_config();
aquarium::TemperatureEngine engine(runtime_config.policy);
aquarium::ActiveEventSnapshot active_events;
aquarium::firmware::Ds18b20Reader reader(kOneWirePin, runtime_config);
aquarium::firmware::BarkNotifier bark;
aquarium::firmware::HeartbeatNotifier heartbeat;
aquarium::DeliveryCoordinator delivery(16, runtime_config.bark_enabled,
                                       runtime_config.mqtt_enabled);
std::array<aquarium::TemperatureEngine, 3> auxiliary_engines{
    aquarium::TemperatureEngine(runtime_config.auxiliary_tanks[0].policy),
    aquarium::TemperatureEngine(runtime_config.auxiliary_tanks[1].policy),
    aquarium::TemperatureEngine(runtime_config.auxiliary_tanks[2].policy)};
aquarium::transport::ScopedEventOutbox auxiliary_delivery(16);
aquarium::transport::DailySummaryScheduler daily_summary;
aquarium::extension_lan::State extension_state{};
aquarium::extension_lan::ConnectivityTracker extension_connectivity_tracker{};
bool auxiliary_turn = false;
bool time_sync_completed = false;
aquarium::RetryBackoff wifi_backoff(1000U, 60000U);
aquarium::heartbeat::HeartbeatSchedule heartbeat_schedule(
    runtime_config.heartbeat_interval_ms, 5000U,
    runtime_config.heartbeat_interval_ms);
#if !defined(AQUARIUM_DISABLE_TAB5_LAN)
WiFiUDP tab5_lan_udp;
std::uint64_t tab5_source_id{};
std::uint32_t tab5_sequence{};
constexpr std::uint16_t kTab5LanPort = 35111U;

std::array<std::uint8_t, 32> tab5_lan_key() {
  std::array<std::uint8_t, 32> key{};
  std::copy_n(kTab5LanStateKey, key.size(), key.begin());
  return key;
}

std::array<aquarium::tab5::ThermalState, 3> auxiliary_thermal_states(
    const aquarium::firmware::Ds18b20Reader::TemperatureReadings& readings) {
  std::array<aquarium::tab5::ThermalState, 3> states{
      aquarium::tab5::ThermalState::no_signal,
      aquarium::tab5::ThermalState::no_signal,
      aquarium::tab5::ThermalState::no_signal};
  for (std::size_t index = 0U; index < states.size(); ++index) {
    if (readings.auxiliary_states[index] !=
        aquarium::firmware::SensorBindingState::valid) {
      continue;
    }
    const auto status = aquarium::transport::temperature_reading_status(
        readings.auxiliary_c[index], runtime_config.auxiliary_tanks[index].policy);
    switch (status) {
      case aquarium::transport::TemperatureReadingStatus::normal:
        states[index] = aquarium::tab5::ThermalState::normal;
        break;
      case aquarium::transport::TemperatureReadingStatus::low:
      case aquarium::transport::TemperatureReadingStatus::low_critical:
        states[index] = aquarium::tab5::ThermalState::low;
        break;
      case aquarium::transport::TemperatureReadingStatus::high:
      case aquarium::transport::TemperatureReadingStatus::high_critical:
        states[index] = aquarium::tab5::ThermalState::high;
        break;
      case aquarium::transport::TemperatureReadingStatus::invalid:
        states[index] = aquarium::tab5::ThermalState::no_signal;
        break;
    }
  }
  return states;
}

void publish_tab5_lan_state(
    const aquarium::firmware::Ds18b20Reader::TemperatureReadings& readings) {
  if (WiFi.status() != WL_CONNECTED) return;
  const auto state = aquarium::tab5::make_lan_state(
      readings.primary, readings.auxiliary_c, auxiliary_thermal_states(readings),
      active_events);
  const auto packet = aquarium::tab5::encode_packet(
      state, tab5_source_id, ++tab5_sequence, tab5_lan_key());
  if (!tab5_lan_udp.beginPacket(IPAddress(255, 255, 255, 255), kTab5LanPort)) {
    return;
  }
  tab5_lan_udp.write(packet.data(), packet.size());
  tab5_lan_udp.endPacket();
}
#endif

std::uint64_t monotonic_millis() {
  static std::uint32_t previous = 0;
  static std::uint64_t epoch = 0;
  const std::uint32_t current = millis();
  if (current < previous) {
    epoch += (1ULL << 32U);
  }
  previous = current;
  return epoch + current;
}

void observe_time_sync() {
  if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
    time_sync_completed = true;
  }
}

std::optional<std::time_t> event_calendar_time(std::time_t sampled_at) {
  if (!time_sync_completed || sampled_at < kMinimumReasonableEpochSeconds) {
    return std::nullopt;
  }
  return sampled_at;
}

void print_temperature(const char* label, const std::optional<double>& value) {
  Serial.print(label);
  Serial.print('=');
  if (value.has_value()) {
    Serial.print(*value, 2);
    Serial.print("C");
  } else {
    Serial.print("invalid");
  }
}

const char* binding_state_name(
    aquarium::firmware::SensorBindingState state) {
  switch (state) {
    case aquarium::firmware::SensorBindingState::valid:
      return "valid";
    case aquarium::firmware::SensorBindingState::unconfigured:
      return "unconfigured";
    case aquarium::firmware::SensorBindingState::duplicate:
      return "duplicate";
  }
  return "unknown";
}

void print_readings(
    const aquarium::firmware::Ds18b20Reader::TemperatureReadings& readings) {
  Serial.print("sample at_ms=");
  Serial.print(readings.at_ms);
  Serial.print(' ');
  print_temperature("main_tank", readings.primary.display_c);
  Serial.print(' ');
  print_temperature("sump_tank", readings.primary.return_c);
  for (std::size_t index = 0; index < readings.auxiliary_c.size(); ++index) {
    Serial.print(' ');
    print_temperature(runtime_config.auxiliary_tanks[index].key,
                      readings.auxiliary_c[index]);
    Serial.print(" binding=");
    Serial.print(binding_state_name(readings.auxiliary_states[index]));
  }
  Serial.println();
}

aquarium::transport::LocalDateTime local_time_from_epoch(std::time_t epoch) {
  constexpr std::time_t kShanghaiOffsetSeconds = 8 * 60 * 60;
  const std::time_t local_epoch = epoch + kShanghaiOffsetSeconds;
  std::tm broken_down{};
  if (gmtime_r(&local_epoch, &broken_down) == nullptr) {
    return {};
  }
  return aquarium::transport::LocalDateTime{
      broken_down.tm_year + 1900,
      broken_down.tm_mon + 1,
      broken_down.tm_mday,
      broken_down.tm_hour,
      broken_down.tm_min,
      broken_down.tm_sec,
      true};
}

aquarium::transport::DailyTemperatureSnapshot daily_snapshot(
    const aquarium::firmware::Ds18b20Reader::TemperatureReadings& readings,
    std::time_t sampled_at) {
  aquarium::transport::DailyTemperatureSnapshot snapshot{};
  if (sampled_at >= 1700000000) {
    snapshot.sampled_at = local_time_from_epoch(sampled_at);
  }
  snapshot.main_c = readings.primary.display_c;
  snapshot.sump_c = readings.primary.return_c;
  snapshot.main_status = aquarium::transport::temperature_reading_status(
      snapshot.main_c, runtime_config.policy);
  for (std::size_t index = 0; index < readings.auxiliary_c.size(); ++index) {
    snapshot.auxiliary_c[index] = readings.auxiliary_c[index];
    switch (readings.auxiliary_states[index]) {
      case aquarium::firmware::SensorBindingState::valid:
        snapshot.auxiliary_states[index] =
            readings.auxiliary_c[index].has_value()
                ? aquarium::transport::SummaryReadingState::valid
                : aquarium::transport::SummaryReadingState::invalid;
        break;
      case aquarium::firmware::SensorBindingState::unconfigured:
        snapshot.auxiliary_states[index] =
            aquarium::transport::SummaryReadingState::unconfigured;
        break;
      case aquarium::firmware::SensorBindingState::duplicate:
        snapshot.auxiliary_states[index] =
            aquarium::transport::SummaryReadingState::configuration_error;
        break;
    }
    if (snapshot.auxiliary_states[index] ==
        aquarium::transport::SummaryReadingState::valid) {
      snapshot.auxiliary_status[index] =
          aquarium::transport::temperature_reading_status(
              snapshot.auxiliary_c[index],
              runtime_config.auxiliary_tanks[index].policy);
    }
  }
  // Slot 0 remains reserved no-signal; only slot 1 is the pleco tank.
  const std::size_t index = aquarium::extension_lan::kPlecoSlot;
  auto& extension = snapshot.extension_tanks[index];
  extension.temperature_c =
      extension_state.temperature_c[index].has_value()
          ? std::optional<double>{*extension_state.temperature_c[index]}
          : std::nullopt;
  extension.state = extension.temperature_c.has_value()
                        ? aquarium::transport::SummaryReadingState::valid
                        : aquarium::transport::SummaryReadingState::invalid;
  extension.fresh = extension_state.fresh;
  if (extension.state == aquarium::transport::SummaryReadingState::valid) {
    extension.status =
        extension_state.thermal_state[index] ==
                aquarium::extension_lan::ThermalState::high
            ? aquarium::transport::TemperatureReadingStatus::high
            : extension_state.thermal_state[index] ==
                      aquarium::extension_lan::ThermalState::low
                  ? aquarium::transport::TemperatureReadingStatus::low
                  : aquarium::transport::TemperatureReadingStatus::normal;
  }
  return snapshot;
}

void connect_wifi(std::uint64_t now_ms) {
  static bool was_connected = false;
  static bool scan_completed = false;
  if (WiFi.status() == WL_CONNECTED) {
    if (!was_connected) {
      Serial.print("wifi connected ip=");
      Serial.println(WiFi.localIP());
      was_connected = true;
    }
    wifi_backoff.record_success();
    return;
  }
  if (was_connected) {
    Serial.println("wifi disconnected");
    was_connected = false;
  }
  if (!wifi_backoff.should_attempt(now_ms)) {
    return;
  }
  WiFi.mode(WIFI_STA);
  if (!scan_completed) {
    const int network_count = WiFi.scanNetworks(false, true);
    bool configured_ssid_seen = false;
    for (int index = 0; index < network_count; ++index) {
      if (WiFi.SSID(index) == aquarium::secrets::kWifiSsid) {
        configured_ssid_seen = true;
        break;
      }
    }
    Serial.print("wifi scan count=");
    Serial.print(network_count);
    Serial.print(" configured_ssid_seen=");
    Serial.println(configured_ssid_seen ? 1 : 0);
    WiFi.scanDelete();
    scan_completed = true;
  }
  Serial.print("wifi connecting status=");
  Serial.println(static_cast<int>(WiFi.status()));
  WiFi.begin(aquarium::secrets::kWifiSsid, aquarium::secrets::kWifiPassword);
  wifi_backoff.record_failure(now_ms);
}

}  // namespace

void setup() {
  Serial.begin(115200);
  connect_wifi(monotonic_millis());
  heartbeat.begin_time_sync();
  reader.begin();
  aquarium::extension_lan::begin();
#if !defined(AQUARIUM_DISABLE_TAB5_LAN)
  tab5_source_id = (static_cast<std::uint64_t>(esp_random()) << 32U) |
                   static_cast<std::uint64_t>(esp_random());
#endif
}

void loop() {
  static std::uint64_t last_sample_at_ms = 0;
  const std::uint64_t now_ms = monotonic_millis();

  connect_wifi(now_ms);
  aquarium::extension_lan::tick(now_ms);
  observe_time_sync();
  const auto latest_extension_state =
      aquarium::extension_lan::snapshot(now_ms);
  if (runtime_config.bark_enabled && WiFi.status() == WL_CONNECTED) {
    const auto transition = aquarium::extension_lan::observeConnectivity(
        &extension_connectivity_tracker, latest_extension_state);
    if (transition != aquarium::extension_lan::ConnectivityEvent::none) {
      const std::time_t event_epoch = std::time(nullptr);
      const auto event_time =
          event_epoch >= kMinimumReasonableEpochSeconds
              ? std::optional<std::time_t>{event_epoch}
              : std::nullopt;
      const auto event_type =
          transition == aquarium::extension_lan::ConnectivityEvent::offline
              ? aquarium::transport::ExtensionConnectivityEvent::offline
              : aquarium::transport::ExtensionConnectivityEvent::recovered;
      const auto message = aquarium::transport::extension_connectivity_message(
          event_type, aquarium::extension_lan::kFreshnessMs, event_time,
          event_time);
      const bool delivered = bark.notify(message);
      Serial.println(delivered ? "extension connectivity bark delivered"
                               : "extension connectivity bark failed");
    }
  }

  if (now_ms - last_sample_at_ms < runtime_config.sample_interval_ms) {
    delay(50);
    return;
  }
  last_sample_at_ms = now_ms;

  const auto readings = reader.read(now_ms);
  extension_state = latest_extension_state;
  const auto& sample = readings.primary;
  observe_time_sync();
  const std::time_t sampled_at = std::time(nullptr);
  const auto event_time = event_calendar_time(sampled_at);
  print_readings(readings);
  const auto events = engine.ingest(sample);
  active_events.apply(events);
#if !defined(AQUARIUM_DISABLE_TAB5_LAN)
  publish_tab5_lan_state(readings);
#endif
  for (const auto& event : events) {
    delivery.enqueue(event, event_time);
  }

  for (std::size_t index = 0; index < auxiliary_engines.size(); ++index) {
    if (readings.auxiliary_states[index] !=
        aquarium::firmware::SensorBindingState::valid) {
      continue;
    }
    const auto auxiliary_events = auxiliary_engines[index].ingest(
        aquarium::TemperatureSample{now_ms, readings.auxiliary_c[index],
                                    std::nullopt});
    for (const auto& event : auxiliary_events) {
      if (!runtime_config.bark_enabled) {
        continue;
      }
      auxiliary_delivery.push({runtime_config.auxiliary_tanks[index].key,
                               runtime_config.auxiliary_tanks[index].label,
                               event,
                               event_time});
    }
  }

  if (runtime_config.bark_enabled && sampled_at >= 1700000000) {
    const auto local_time = local_time_from_epoch(sampled_at);
    daily_summary.observe(local_time, monotonic_millis(),
                          daily_snapshot(readings, sampled_at));
  }

  bool attempted_alert = false;
  const auto* main_event = delivery.bark_front();
  const auto* auxiliary_event = auxiliary_delivery.front();
  if (runtime_config.bark_enabled &&
      (main_event != nullptr || auxiliary_event != nullptr)) {
    attempted_alert = true;
    const bool choose_auxiliary =
        auxiliary_event != nullptr &&
        (main_event == nullptr || auxiliary_turn);
    if (choose_auxiliary) {
      const bool delivered = bark.notify(*auxiliary_event,
                                         runtime_config.aquarium_id);
      Serial.println(delivered ? "auxiliary bark delivered"
                               : "auxiliary bark failed");
      if (delivered) {
        auxiliary_delivery.pop();
      }
      auxiliary_turn = false;
    } else {
      const auto* main_record = delivery.bark_record_front();
      const bool delivered =
          main_record != nullptr &&
          bark.notify(main_record->event, runtime_config.aquarium_id,
                      main_record->event_time);
      Serial.println(delivered ? "main bark delivered" : "main bark failed");
      delivery.acknowledge_bark(delivered);
      auxiliary_turn = true;
    }
  }
  daily_summary.expire(monotonic_millis());
  if (runtime_config.bark_enabled && !attempted_alert && delivery.bark_front() == nullptr &&
      auxiliary_delivery.front() == nullptr && daily_summary.pending() != nullptr) {
    const bool delivered =
        bark.notify(*daily_summary.pending(), runtime_config.aquarium_id);
    Serial.println(delivered ? "daily bark delivered" : "daily bark failed");
    daily_summary.acknowledge(delivered);
  }
  if (runtime_config.heartbeat_enabled && WiFi.status() == WL_CONNECTED &&
      heartbeat_schedule.should_attempt(now_ms)) {
    std::optional<aquarium::heartbeat::TemperatureSnapshot>
        temperature_snapshot;
    if (sampled_at >= 1700000000) {
      const aquarium::transport::DailyTemperatureSummary summary{
          0U, aquarium::transport::DailySlot::morning,
          daily_snapshot(readings, sampled_at)};
      temperature_snapshot = aquarium::heartbeat::TemperatureSnapshot{
          static_cast<std::uint64_t>(sampled_at) * 1000ULL,
          aquarium::transport::daily_summary_body(summary)};
    }
    const bool delivered = heartbeat.notify(sample, active_events, now_ms,
                                            temperature_snapshot);
    Serial.println(delivered ? "heartbeat delivered" : "heartbeat failed");
    if (delivered) {
      heartbeat_schedule.record_success(now_ms);
    } else {
      heartbeat_schedule.record_failure(now_ms);
    }
  }
}
