#include "aliyun_mqtt.hpp"

#include "secrets.hpp"

#include "transport_contract.hpp"

namespace aquarium::firmware {
namespace {

constexpr char kTelemetrySuffix[] = "/user/aquarium/telemetry";
constexpr char kEventsSuffix[] = "/user/aquarium/events";

String topic(const char* suffix) {
  return String("/") + secrets::kAliyunProductKey + "/" +
         secrets::kAliyunDeviceName + suffix;
}

}  // namespace

AliyunMqtt::AliyunMqtt() : mqtt_client_(tls_client_) {
  tls_client_.setCACert(secrets::kAliyunRootCaPem);
  tls_client_.setTimeout(5000);
  mqtt_client_.setServer(secrets::kAliyunHost, secrets::kAliyunPort);
  mqtt_client_.setBufferSize(768);
}

bool AliyunMqtt::connect(std::uint64_t now_ms) {
  if (mqtt_client_.connected()) {
    reconnect_backoff_.record_success();
    return true;
  }
  if (secrets::kAliyunRootCaPem[0] == '\0' ||
      !reconnect_backoff_.should_attempt(now_ms)) {
    return false;
  }
  const bool connected = mqtt_client_.connect(
      secrets::kAliyunClientId, secrets::kAliyunUsername, secrets::kAliyunPassword);
  if (connected) {
    reconnect_backoff_.record_success();
  } else {
    reconnect_backoff_.record_failure(now_ms);
  }
  return connected;
}

bool AliyunMqtt::publish_telemetry(const TemperatureSample& sample,
                                   std::uint64_t now_ms) {
  if (!connect(now_ms)) {
    return false;
  }
  const String encoded = transport::telemetry_json(sample).c_str();
  return mqtt_client_.publish(topic(kTelemetrySuffix).c_str(), encoded.c_str());
}

bool AliyunMqtt::publish_event(const TemperatureEvent& event,
                               std::uint64_t now_ms) {
  if (!connect(now_ms)) {
    return false;
  }
  const String encoded = transport::event_json(event).c_str();
  return mqtt_client_.publish(topic(kEventsSuffix).c_str(), encoded.c_str());
}

void AliyunMqtt::loop() { mqtt_client_.loop(); }

}  // namespace aquarium::firmware
