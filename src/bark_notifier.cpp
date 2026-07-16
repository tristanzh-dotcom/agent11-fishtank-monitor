#include "bark_notifier.hpp"

#include "secrets.hpp"
#include "transport_contract.hpp"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

namespace aquarium::firmware {
namespace {

constexpr char kBarkPushUrl[] = "https://api.day.app/push";

}  // namespace

bool BarkNotifier::notify(const TemperatureEvent& event,
                          const char* aquarium_id) {
  if (secrets::kBarkDeviceKey[0] == '\0' || secrets::kBarkRootCaPem[0] == '\0') {
    return false;
  }

  const auto message = transport::bark_message(event, aquarium_id);
  const String payload =
      transport::bark_request_json(message, secrets::kBarkDeviceKey).c_str();
  WiFiClientSecure client;
  client.setCACert(secrets::kBarkRootCaPem);
  client.setTimeout(5000);
  HTTPClient http;
  http.setConnectTimeout(5000);
  if (!http.begin(client, kBarkPushUrl)) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  const int status = http.POST(payload);
  http.end();
  return status >= 200 && status < 300;
}

}  // namespace aquarium::firmware
