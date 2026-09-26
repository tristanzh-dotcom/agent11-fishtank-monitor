#include "lan_air_probe.hpp"
#include "lan_air_frame.hpp"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wifi.h>
#include <cstring>

namespace aquarium::diagnostics {
namespace {
struct Record {
  AirFrame frame{};
  std::uint32_t at_ms{};
  std::uint16_t length{};
  std::int8_t rssi{};
};
constexpr unsigned kCapacity = 64U;
Record queue[kCapacity]{};
unsigned head{}, tail{}, dropped{};
std::uint8_t target[6]{}, ap[6]{};
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
bool active{};
std::uint32_t started{};

void receive(void* buffer, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_DATA || !buffer) return;
  const auto* packet = static_cast<const wifi_promiscuous_pkt_t*>(buffer);
  Record record{};
  portENTER_CRITICAL(&lock);
  if (!active || !selectAirFrame(packet->payload, packet->rx_ctrl.sig_len,
                                 target, ap, &record.frame)) {
    portEXIT_CRITICAL(&lock);
    return;
  }
  record.at_ms = millis();
  record.length = packet->rx_ctrl.sig_len;
  record.rssi = packet->rx_ctrl.rssi;
  const auto next = (head + 1U) % kCapacity;
  if (next == tail) ++dropped;
  else { queue[head] = record; head = next; }
  portEXIT_CRITICAL(&lock);
}

void stop() {
  // Disable acceptance under the same lock used by the Wi-Fi callback.
  portENTER_CRITICAL(&lock);
  active = false;
  portEXIT_CRITICAL(&lock);
  const auto result = esp_wifi_set_promiscuous(false);
  esp_wifi_set_promiscuous_rx_cb(nullptr);
  Serial.printf("AIR_STOP result=%d dropped=%u\n", result, dropped);
}

int hex(char c) {
  if (c >= '0' && c <= '9') return c-'0';
  if (c >= 'a' && c <= 'f') return c-'a'+10;
  if (c >= 'A' && c <= 'F') return c-'A'+10;
  return -1;
}
}  // namespace

bool airCommand(const char* command) {
  if (std::strcmp(command, "AIR_OFF") == 0) { stop(); return true; }
  if (std::strncmp(command, "AIR ", 4U) != 0) return false;
  std::uint8_t selected[6]{};
  if (std::strlen(command) != 16U || WiFi.status() != WL_CONNECTED) {
    Serial.println("AIR_BLOCKED"); return true;
  }
  for (unsigned i=0; i<6U; ++i) {
    const auto high = hex(command[4U+i*2U]), low = hex(command[5U+i*2U]);
    if (high < 0 || low < 0) { Serial.println("AIR_BLOCKED"); return true; }
    selected[i] = (high << 4U) | low;
  }
  if (active) stop();
  wifi_ap_record_t info{};
  if (esp_wifi_sta_get_ap_info(&info) != ESP_OK) {
    Serial.println("AIR_BLOCKED"); return true;
  }
  portENTER_CRITICAL(&lock);
  std::memcpy(target, selected, sizeof(target));
  std::memcpy(ap, info.bssid, sizeof(ap));
  head = tail = dropped = 0U;
  active = true;
  portEXIT_CRITICAL(&lock);
  wifi_promiscuous_filter_t filter{};
  filter.filter_mask = WIFI_PROMIS_FILTER_MASK_DATA;
  if (esp_wifi_set_promiscuous_filter(&filter) != ESP_OK ||
      esp_wifi_set_promiscuous_rx_cb(receive) != ESP_OK ||
      esp_wifi_set_promiscuous(true) != ESP_OK) {
    stop(); Serial.println("AIR_BLOCKED"); return true;
  }
  started = millis();
  Serial.printf("AIR_START channel=%u auth=%u window_ms=600000\n",
                info.primary, static_cast<unsigned>(info.authmode));
  return true;
}

void tickAirProbe() {
  if (active && (millis()-started >= 600000U || WiFi.status()!=WL_CONNECTED)) stop();
  // Fixed work bound; no allocation or serial output from the Wi-Fi task.
  for (unsigned i=0; i<8U; ++i) {
    Record record{};
    portENTER_CRITICAL(&lock);
    if (tail == head) { portEXIT_CRITICAL(&lock); break; }
    record = queue[tail]; tail = (tail + 1U) % kCapacity;
    portEXIT_CRITICAL(&lock);
    Serial.printf("AIR_FRAME uptime_ms=%lu direction=%s group=%u associated_ap=%u retry=%u encrypted=%u dot11_seq=%u length=%u rssi=%d\n",
                  static_cast<unsigned long>(record.at_ms),
                  record.frame.downlink ? "down" : "up", record.frame.group,
                  record.frame.associated_ap, record.frame.retry,
                  record.frame.encrypted, record.frame.sequence,
                  record.length, record.rssi);
  }
}
}  // namespace aquarium::diagnostics
