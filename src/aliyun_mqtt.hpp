#pragma once

#include "temperature_engine.hpp"
#include "retry_backoff.hpp"

#include <PubSubClient.h>
#include <WiFiClientSecure.h>

namespace aquarium::firmware {

class AliyunMqtt {
 public:
  AliyunMqtt();

  bool publish_telemetry(const TemperatureSample& sample, std::uint64_t now_ms);
  bool publish_event(const TemperatureEvent& event, std::uint64_t now_ms);
  void loop();

 private:
  WiFiClientSecure tls_client_;
  PubSubClient mqtt_client_;
  RetryBackoff reconnect_backoff_{1000U, 60000U};

  bool connect(std::uint64_t now_ms);
};

}  // namespace aquarium::firmware
