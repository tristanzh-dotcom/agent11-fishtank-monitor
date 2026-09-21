#include "bark_notifier.hpp"

#include "secrets.hpp"
#include "transport_contract.hpp"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include <ctime>

namespace aquarium::firmware {
namespace {

constexpr char kBarkPushUrl[] = "https://api.day.app/push";

}  // namespace

bool BarkNotifier::notify(const TemperatureEvent& event,
                          const char* aquarium_id,
                          std::optional<std::time_t> event_time) {
  if (secrets::kBarkDeviceKey[0] == '\0' || secrets::kBarkRootCaPem[0] == '\0') {
    return false;
  }

  const auto message = transport::bark_message(
      event, aquarium_id, event_time, std::time(nullptr), "温控ESP1号");
  const String payload =
      transport::bark_request_json(message, secrets::kBarkDeviceKey).c_str();
  WiFiClientSecure client;
  client.setCACert(secrets::kBarkRootCaPem);
  client.setTimeout(5000);
  client.setHandshakeTimeout(5);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  if (!http.begin(client, kBarkPushUrl)) {
    Serial.println("bark http begin failed");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  const int status = http.POST(payload);
  http.end();
  Serial.printf("bark http status=%d\n", status);
  return status >= 200 && status < 300;
}

bool BarkNotifier::notify(const transport::ScopedTemperatureEvent& event,
                          const char* aquarium_id,
                          std::optional<std::time_t> event_time) {
  if (secrets::kBarkDeviceKey[0] == '\0' || secrets::kBarkRootCaPem[0] == '\0') {
    return false;
  }
  const auto message = transport::bark_message(
      event, aquarium_id, event_time, std::time(nullptr), "温控ESP1号");
  const String payload =
      transport::bark_request_json(message, secrets::kBarkDeviceKey).c_str();
  WiFiClientSecure client;
  client.setCACert(secrets::kBarkRootCaPem);
  client.setTimeout(5000);
  client.setHandshakeTimeout(5);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  if (!http.begin(client, kBarkPushUrl)) {
    Serial.println("bark http begin failed");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  const int status = http.POST(payload);
  http.end();
  Serial.printf("bark http status=%d\n", status);
  return status >= 200 && status < 300;
}

bool BarkNotifier::notify(const transport::DailyTemperatureSummary& summary,
                          const char* aquarium_id) {
  if (secrets::kBarkDeviceKey[0] == '\0' || secrets::kBarkRootCaPem[0] == '\0') {
    return false;
  }
  const auto message = transport::daily_summary_message(summary, aquarium_id);
  const String payload =
      transport::bark_request_json(message, secrets::kBarkDeviceKey).c_str();
  WiFiClientSecure client;
  client.setCACert(secrets::kBarkRootCaPem);
  client.setTimeout(5000);
  client.setHandshakeTimeout(5);
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);
  if (!http.begin(client, kBarkPushUrl)) {
    Serial.println("bark http begin failed");
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  const int status = http.POST(payload);
  http.end();
  Serial.printf("bark http status=%d\n", status);
  return status >= 200 && status < 300;
}

}  // namespace aquarium::firmware
