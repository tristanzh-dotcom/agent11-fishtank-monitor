#include "delivery_coordinator.hpp"

namespace aquarium {

bool DeliveryCoordinator::enqueue(const TemperatureEvent& event) {
  bool queued = false;
  if (bark_enabled_) {
    queued = bark_outbox_.push(event);
  }
  if (mqtt_enabled_) {
    queued = mqtt_outbox_.push(event) || queued;
  }
  return queued;
}

const TemperatureEvent* DeliveryCoordinator::bark_front() const {
  return bark_enabled_ ? bark_outbox_.front() : nullptr;
}

const TemperatureEvent* DeliveryCoordinator::mqtt_front() const {
  return mqtt_enabled_ ? mqtt_outbox_.front() : nullptr;
}

void DeliveryCoordinator::acknowledge_bark(bool delivered) {
  if (bark_enabled_ && delivered) {
    bark_outbox_.pop();
  }
}

void DeliveryCoordinator::acknowledge_mqtt(bool delivered) {
  if (mqtt_enabled_ && delivered) {
    mqtt_outbox_.pop();
  }
}

}  // namespace aquarium
