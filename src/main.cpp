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
#include "grass_lan_state.hpp"
#if !defined(AQUARIUM_DISABLE_TAB5_LAN)
#include "tab5_lan_state.hpp"
#endif

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <esp_system.h>
#if !defined(AQUARIUM_DISABLE_TAB5_LAN)
#include <WiFiUdp.h>
#endif

#include <array>
#include <algorithm>
#include <cstdint>
#include <ctime>
#include <cstring>
#include <optional>

namespace {

#if !defined(AQUARIUM_DISABLE_TAB5_LAN)
#include "tab5_lan_secret.hpp"
#endif

constexpr char kFirmwareVersion[] = "esp1-lan-rx-20260924.1";
constexpr std::uint8_t kOneWirePin = 4;
constexpr std::time_t kMinimumReasonableEpochSeconds = 1700000000;
constexpr std::size_t kDiagnosticLogCapacity = 32U;
constexpr std::uint32_t kDiagnosticStorageMagic = 0x44494147U;
constexpr std::uint32_t kDiagnosticStorageVersion = 1U;
constexpr std::uint64_t kDiagnosticLoopGapMs = 10000U;

enum class DiagnosticEvent : std::uint8_t {
  boot,
  wifi_down,
  wifi_up,
  loop_gap,
  extension_first_packet,
  extension_sender_session,
  extension_stale,
  extension_recovered,
  // Append only: preserve the existing persisted event values and record layout.
  extension_receive_gap,
  grass_first_packet,
  grass_sender_session,
  grass_receive_gap,
  grass_stale,
  grass_recovered,
};

struct DiagnosticRecord {
  std::uint32_t uptime_ms{};
  std::uint32_t epoch_seconds{};
  std::uint32_t value{};
  std::uint32_t detail{};
  DiagnosticEvent event{};
};

struct DiagnosticStorage {
  std::uint32_t magic{};
  std::uint32_t version{};
  std::uint32_t next{};
  std::uint32_t count{};
  std::uint32_t overwritten{};
  std::array<DiagnosticRecord, kDiagnosticLogCapacity> records{};
};
static_assert(sizeof(DiagnosticStorage) <= 1024U,
              "connectivity diagnostics must stay small");

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
aquarium::grass_lan::State grass_state{};
aquarium::grass_lan::ConnectivityTracker grass_connectivity_tracker{};
bool auxiliary_turn = false;
bool time_sync_completed = false;
aquarium::RetryBackoff wifi_backoff(1000U, 60000U);
aquarium::heartbeat::HeartbeatSchedule heartbeat_schedule(
    runtime_config.heartbeat_interval_ms, 5000U,
    runtime_config.heartbeat_interval_ms);
std::array<DiagnosticRecord, kDiagnosticLogCapacity> diagnostic_records{};
std::size_t diagnostic_next{};
std::size_t diagnostic_count{};
std::uint32_t diagnostic_overwritten{};
std::uint64_t diagnostic_last_receive_at_ms{};
bool diagnostic_received_packet{};
std::uint64_t diagnostic_grass_last_receive_at_ms{};
bool diagnostic_grass_received_packet{};
bool diagnostic_dirty{};
bool diagnostic_storage_ok{};
bool diagnostic_wifi_seen_connected{};
bool diagnostic_wifi_down_pending{};
std::uint64_t diagnostic_wifi_down_since_ms{};
std::uint64_t diagnostic_last_loop_ms{};
bool diagnostic_has_loop_time{};

void load_diagnostic_log() {
  Preferences preferences;
  if (!preferences.begin("net_diag", false)) {
    diagnostic_storage_ok = false;
    Serial.println("DIAG_STORAGE unavailable");
    return;
  }
  diagnostic_storage_ok = true;
  if (preferences.isKey("ring")) {
    DiagnosticStorage stored{};
    const bool loaded =
        preferences.getBytesLength("ring") == sizeof(stored) &&
        preferences.getBytes("ring", &stored, sizeof(stored)) == sizeof(stored);
    if (loaded && stored.magic == kDiagnosticStorageMagic &&
        stored.version == kDiagnosticStorageVersion &&
        stored.next < kDiagnosticLogCapacity &&
        stored.count <= kDiagnosticLogCapacity) {
      diagnostic_records = stored.records;
      diagnostic_next = stored.next;
      diagnostic_count = stored.count;
      diagnostic_overwritten = stored.overwritten;
    } else {
      Serial.println("DIAG_STORAGE invalid");
    }
  }
  preferences.end();
}

void persist_diagnostic_log() {
  if (!diagnostic_dirty) return;
  DiagnosticStorage stored{};
  stored.magic = kDiagnosticStorageMagic;
  stored.version = kDiagnosticStorageVersion;
  stored.next = static_cast<std::uint32_t>(diagnostic_next);
  stored.count = static_cast<std::uint32_t>(diagnostic_count);
  stored.overwritten = diagnostic_overwritten;
  stored.records = diagnostic_records;
  diagnostic_dirty = false;

  Preferences preferences;
  if (!preferences.begin("net_diag", false)) {
    diagnostic_storage_ok = false;
    Serial.println("DIAG_STORAGE write_unavailable");
    return;
  }
  const std::size_t written = preferences.putBytes("ring", &stored,
                                                    sizeof(stored));
  preferences.end();
  diagnostic_storage_ok = written == sizeof(stored);
  if (!diagnostic_storage_ok) Serial.println("DIAG_STORAGE write_failed");
}

void record_diagnostic(DiagnosticEvent event, std::uint64_t uptime_ms,
                       std::uint32_t value = 0U,
                       std::uint32_t detail = 0U) {
  const std::time_t wall_time = std::time(nullptr);
  DiagnosticRecord record{};
  record.uptime_ms = static_cast<std::uint32_t>(uptime_ms);
  record.epoch_seconds =
      wall_time >= kMinimumReasonableEpochSeconds
          ? static_cast<std::uint32_t>(wall_time)
          : 0U;
  record.value = value;
  record.detail = detail;
  record.event = event;
  diagnostic_records[diagnostic_next] = record;
  diagnostic_next = (diagnostic_next + 1U) % diagnostic_records.size();
  if (diagnostic_count < diagnostic_records.size()) {
    ++diagnostic_count;
  } else {
    ++diagnostic_overwritten;
  }
  diagnostic_dirty = true;
}

void dump_diagnostic_log() {
  Serial.printf("DIAG_BEGIN count=%u overwritten=%lu storage=%s\n",
                static_cast<unsigned>(diagnostic_count),
                static_cast<unsigned long>(diagnostic_overwritten),
                diagnostic_storage_ok ? "ok" : "unavailable");
  const std::size_t oldest =
      (diagnostic_next + diagnostic_records.size() - diagnostic_count) %
      diagnostic_records.size();
  for (std::size_t offset = 0U; offset < diagnostic_count; ++offset) {
    const auto& record = diagnostic_records[(oldest + offset) %
                                             diagnostic_records.size()];
    const auto epoch = static_cast<unsigned long>(record.epoch_seconds);
    const auto uptime = static_cast<unsigned long>(record.uptime_ms);
    switch (record.event) {
      case DiagnosticEvent::boot:
        Serial.printf("DIAG event=BOOT epoch_s=%lu uptime_ms=%lu reset_reason=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value));
        break;
      case DiagnosticEvent::wifi_down:
        Serial.printf("DIAG event=WIFI_DOWN epoch_s=%lu uptime_ms=%lu\n",
                      epoch, uptime);
        break;
      case DiagnosticEvent::wifi_up:
        if (record.value == UINT32_MAX) {
          Serial.printf("DIAG event=WIFI_UP epoch_s=%lu uptime_ms=%lu reconnect_ms=na\n",
                        epoch, uptime);
        } else {
          Serial.printf("DIAG event=WIFI_UP epoch_s=%lu uptime_ms=%lu reconnect_ms=%lu\n",
                        epoch, uptime,
                        static_cast<unsigned long>(record.value));
        }
        break;
      case DiagnosticEvent::loop_gap:
        Serial.printf("DIAG event=LOOP_GAP epoch_s=%lu uptime_ms=%lu gap_ms=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value));
        break;
      case DiagnosticEvent::extension_first_packet:
        Serial.printf("DIAG event=EXT_FIRST_PACKET epoch_s=%lu uptime_ms=%lu sequence=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value));
        break;
      case DiagnosticEvent::extension_sender_session:
        Serial.printf("DIAG event=EXT_SENDER_SESSION epoch_s=%lu uptime_ms=%lu first_sequence=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value));
        break;
      case DiagnosticEvent::extension_stale:
        Serial.printf("DIAG event=EXT_STALE epoch_s=%lu uptime_ms=%lu age_ms=%lu sequence=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value),
                      static_cast<unsigned long>(record.detail));
        break;
      case DiagnosticEvent::extension_receive_gap:
        Serial.printf("DIAG event=EXT_RECEIVE_GAP epoch_s=%lu uptime_ms=%lu gap_ms=%lu missed_frames=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value),
                      static_cast<unsigned long>(record.detail));
        break;
      case DiagnosticEvent::grass_first_packet:
        Serial.printf("DIAG event=GRASS_FIRST_PACKET epoch_s=%lu uptime_ms=%lu sequence=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value));
        break;
      case DiagnosticEvent::grass_sender_session:
        Serial.printf("DIAG event=GRASS_SENDER_SESSION epoch_s=%lu uptime_ms=%lu first_sequence=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value));
        break;
      case DiagnosticEvent::grass_receive_gap:
        Serial.printf("DIAG event=GRASS_RECEIVE_GAP epoch_s=%lu uptime_ms=%lu gap_ms=%lu missed_frames=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value),
                      static_cast<unsigned long>(record.detail));
        break;
      case DiagnosticEvent::grass_stale:
        Serial.printf("DIAG event=GRASS_STALE epoch_s=%lu uptime_ms=%lu age_ms=%lu sequence=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value),
                      static_cast<unsigned long>(record.detail));
        break;
      case DiagnosticEvent::grass_recovered:
        Serial.printf("DIAG event=GRASS_RECOVERED epoch_s=%lu uptime_ms=%lu silence_ms=%lu missed_frames=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value),
                      static_cast<unsigned long>(record.detail));
        break;
      case DiagnosticEvent::extension_recovered:
        Serial.printf("DIAG event=EXT_RECOVERED epoch_s=%lu uptime_ms=%lu silence_ms=%lu missed_frames=%lu\n",
                      epoch, uptime, static_cast<unsigned long>(record.value),
                      static_cast<unsigned long>(record.detail));
        break;
    }
  }
  const auto extension_rx = aquarium::extension_lan::receiveCounters();
  const auto grass_rx = aquarium::grass_lan::receiveCounters();
  Serial.printf("DIAG firmware=%s counters_scope=boot\n", kFirmwareVersion);
  Serial.printf("DIAG_RX source=ESP2 received=%lu accepted=%lu rejected=%lu\n",
                static_cast<unsigned long>(extension_rx.received),
                static_cast<unsigned long>(extension_rx.accepted),
                static_cast<unsigned long>(extension_rx.rejected));
  Serial.printf("DIAG_RX source=ESP3 received=%lu accepted=%lu rejected=%lu\n",
                static_cast<unsigned long>(grass_rx.received),
                static_cast<unsigned long>(grass_rx.accepted),
                static_cast<unsigned long>(grass_rx.rejected));
  Serial.println("DIAG_END");
}

void process_diagnostic_command() {
  static char command[12]{};
  static std::size_t length{};
  while (Serial.available() > 0) {
    const int input = Serial.read();
    if (input == '\r') continue;
    if (input == '\n') {
      command[length] = '\0';
      if (std::strcmp(command, "DIAG") == 0) dump_diagnostic_log();
      length = 0U;
    } else if (length + 1U < sizeof(command)) {
      command[length++] = static_cast<char>(input);
    } else {
      length = 0U;
    }
  }
}

void record_extension_packet(const aquarium::extension_lan::State& state) {
  if (!state.has_packet ||
      state.received_at_ms == diagnostic_last_receive_at_ms) {
    return;
  }
  if (!diagnostic_received_packet) {
    record_diagnostic(DiagnosticEvent::extension_first_packet,
                      state.received_at_ms, state.sequence);
    diagnostic_received_packet = true;
  } else if (state.sender_session_changed) {
    record_diagnostic(DiagnosticEvent::extension_sender_session,
                      state.received_at_ms, state.sequence);
  }
  if (state.receive_gap_ms > 15000U || state.missed_frames > 0U) {
    record_diagnostic(DiagnosticEvent::extension_receive_gap,
                      state.received_at_ms,
                      static_cast<std::uint32_t>(state.receive_gap_ms),
                      state.missed_frames);
  }
  diagnostic_last_receive_at_ms = state.received_at_ms;
}

void record_grass_packet(const aquarium::grass_lan::State& state) {
  if (!state.has_packet ||
      state.received_at_ms == diagnostic_grass_last_receive_at_ms) return;
  if (!diagnostic_grass_received_packet) {
    record_diagnostic(DiagnosticEvent::grass_first_packet,
                      state.received_at_ms, state.sequence);
    diagnostic_grass_received_packet = true;
  } else if (state.sender_session_changed) {
    record_diagnostic(DiagnosticEvent::grass_sender_session,
                      state.received_at_ms, state.sequence);
  }
  if (state.receive_gap_ms > 45000U || state.missed_frames > 0U) {
    record_diagnostic(DiagnosticEvent::grass_receive_gap,
                      state.received_at_ms,
                      static_cast<std::uint32_t>(state.receive_gap_ms),
                      state.missed_frames);
  }
  diagnostic_grass_last_receive_at_ms = state.received_at_ms;
}
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
  // ESP3 supplies grass in slot 0; ESP2 supplies pleco in slot 1.
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
  auto& grass = snapshot.extension_tanks[0];
  grass.temperature_c = grass_state.temperature_c[0].has_value()
                            ? std::optional<double>{
                                  *grass_state.temperature_c[0]}
                            : std::nullopt;
  grass.state = grass.temperature_c.has_value()
                    ? aquarium::transport::SummaryReadingState::valid
                    : aquarium::transport::SummaryReadingState::invalid;
  grass.fresh = grass_state.fresh;
  if (grass.state == aquarium::transport::SummaryReadingState::valid) {
    grass.status =
        grass_state.thermal_state[0] == aquarium::grass_lan::ThermalState::high
            ? aquarium::transport::TemperatureReadingStatus::high
            : grass_state.thermal_state[0] ==
                      aquarium::grass_lan::ThermalState::low
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
      const std::uint32_t reconnect_ms =
          diagnostic_wifi_down_pending
              ? static_cast<std::uint32_t>(now_ms -
                                           diagnostic_wifi_down_since_ms)
              : UINT32_MAX;
      record_diagnostic(DiagnosticEvent::wifi_up, now_ms, reconnect_ms);
      diagnostic_wifi_seen_connected = true;
      diagnostic_wifi_down_pending = false;
      was_connected = true;
    }
    wifi_backoff.record_success();
    return;
  }
  if (was_connected) {
    Serial.println("wifi disconnected");
    was_connected = false;
    if (diagnostic_wifi_seen_connected) {
      record_diagnostic(DiagnosticEvent::wifi_down, now_ms);
      diagnostic_wifi_down_pending = true;
      diagnostic_wifi_down_since_ms = now_ms;
    }
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
  Serial.printf("ESP1_FIRMWARE_VERSION=%s\n", kFirmwareVersion);
  load_diagnostic_log();
  record_diagnostic(DiagnosticEvent::boot, monotonic_millis(),
                    static_cast<std::uint32_t>(esp_reset_reason()));
  persist_diagnostic_log();
  connect_wifi(monotonic_millis());
  heartbeat.begin_time_sync();
  reader.begin();
  aquarium::extension_lan::begin();
  aquarium::grass_lan::begin();
#if !defined(AQUARIUM_DISABLE_TAB5_LAN)
  tab5_source_id = (static_cast<std::uint64_t>(esp_random()) << 32U) |
                   static_cast<std::uint64_t>(esp_random());
#endif
}

void loop() {
  static std::uint64_t last_sample_at_ms = 0;
  process_diagnostic_command();
  std::uint64_t now_ms = monotonic_millis();
  if (diagnostic_has_loop_time &&
      now_ms - diagnostic_last_loop_ms >= kDiagnosticLoopGapMs) {
    record_diagnostic(
        DiagnosticEvent::loop_gap, now_ms,
        static_cast<std::uint32_t>(now_ms - diagnostic_last_loop_ms));
  }
  diagnostic_last_loop_ms = now_ms;
  diagnostic_has_loop_time = true;

  connect_wifi(now_ms);
  now_ms = monotonic_millis();
  aquarium::extension_lan::tick(now_ms);
  observe_time_sync();
  auto latest_extension_state = aquarium::extension_lan::snapshot(now_ms);
  record_extension_packet(latest_extension_state);
  if (runtime_config.bark_enabled && WiFi.status() == WL_CONNECTED) {
    const auto transition = aquarium::extension_lan::observeConnectivity(
        &extension_connectivity_tracker, latest_extension_state);
    if (transition != aquarium::extension_lan::ConnectivityEvent::none) {
      if (transition == aquarium::extension_lan::ConnectivityEvent::offline) {
        record_diagnostic(
            DiagnosticEvent::extension_stale, now_ms,
            static_cast<std::uint32_t>(now_ms -
                                       latest_extension_state.received_at_ms),
            latest_extension_state.sequence);
      } else {
        record_diagnostic(
            DiagnosticEvent::extension_recovered,
            latest_extension_state.received_at_ms,
            static_cast<std::uint32_t>(
                latest_extension_state.receive_gap_ms),
            latest_extension_state.missed_frames);
      }
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
  // The ESP2 Bark request above is synchronous. Poll ESP3 after it returns,
  // using the current clock, before making this source's connectivity decision.
  now_ms = monotonic_millis();
  aquarium::grass_lan::tick(now_ms);
  grass_state = aquarium::grass_lan::snapshot(now_ms);
  record_grass_packet(grass_state);
  if (runtime_config.bark_enabled && WiFi.status() == WL_CONNECTED) {
    const auto grass_transition =
        aquarium::grass_lan::observeConnectivity(&grass_connectivity_tracker,
                                                 grass_state);
    if (grass_transition != aquarium::grass_lan::ConnectivityEvent::none) {
      if (grass_transition == aquarium::grass_lan::ConnectivityEvent::offline) {
        record_diagnostic(DiagnosticEvent::grass_stale, now_ms,
                          static_cast<std::uint32_t>(now_ms - grass_state.received_at_ms),
                          grass_state.sequence);
      } else {
        record_diagnostic(DiagnosticEvent::grass_recovered,
                          grass_state.received_at_ms,
                          static_cast<std::uint32_t>(grass_state.receive_gap_ms),
                          grass_state.missed_frames);
      }
      const std::time_t event_epoch = std::time(nullptr);
      const auto event_time =
          event_epoch >= kMinimumReasonableEpochSeconds
              ? std::optional<std::time_t>{event_epoch}
              : std::nullopt;
      const auto event_type =
          grass_transition == aquarium::grass_lan::ConnectivityEvent::offline
              ? aquarium::transport::ExtensionConnectivityEvent::offline
              : aquarium::transport::ExtensionConnectivityEvent::recovered;
      const auto message = aquarium::transport::extension_connectivity_message(
          event_type, 75000U, event_time, event_time, "南美草缸", "ESP3",
          "esp3");
      const bool delivered = bark.notify(message);
      Serial.println(delivered ? "ESP3 connectivity bark delivered"
                               : "ESP3 connectivity bark failed");
    }
  }

  // Account for elapsed notification time before sampling and summary freshness.
  now_ms = monotonic_millis();
  latest_extension_state = aquarium::extension_lan::snapshot(now_ms);
  grass_state = aquarium::grass_lan::snapshot(now_ms);
  if (now_ms - last_sample_at_ms < runtime_config.sample_interval_ms) {
    persist_diagnostic_log();
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
  persist_diagnostic_log();
}
