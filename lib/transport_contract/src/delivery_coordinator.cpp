#include "delivery_coordinator.hpp"

namespace aquarium {

bool DeliveryCoordinator::enqueue(const TemperatureEvent& event,
                                  std::optional<std::time_t> event_time) {
  bool queued = false;
  if (bark_enabled_) {
    if (event.state == EventState::resolved &&
        bark_outbox_.size() >= bark_capacity_) {
      remove_pending(event.type);
    }
    if (bark_outbox_.size() >= bark_capacity_) {
      ++bark_dropped_count_;
    } else {
      const auto prepared = bark_policy_.prepare(event, "main_tank");
      if (prepared.has_value()) {
        if (prepared->state == EventState::resolved ||
            prepared->state == EventState::escalated) {
          remove_pending(prepared->type);
        }
        bark_outbox_.push_back(BarkDeliveryRecord{*prepared, event_time});
        queued = true;
      }
    }
  }
  if (mqtt_enabled_) {
    queued = mqtt_outbox_.push(event) || queued;
  }
  return queued;
}

void DeliveryCoordinator::remove_pending(EventType type) {
  const auto problem = transport::BarkAlertPolicy::problem_type(type);
  for (auto it = bark_outbox_.begin(); it != bark_outbox_.end();) {
    if (transport::BarkAlertPolicy::problem_type(it->event.type) == problem) {
      it = bark_outbox_.erase(it);
    } else {
      ++it;
    }
  }
}

const BarkDeliveryRecord* DeliveryCoordinator::bark_record_front() const {
  return bark_enabled_ && !bark_outbox_.empty() ? &bark_outbox_.front()
                                                 : nullptr;
}

const TemperatureEvent* DeliveryCoordinator::bark_front() const {
  const auto* record = bark_record_front();
  return record == nullptr ? nullptr : &record->event;
}

const TemperatureEvent* DeliveryCoordinator::mqtt_front() const {
  return mqtt_enabled_ ? mqtt_outbox_.front() : nullptr;
}

void DeliveryCoordinator::acknowledge_bark(bool delivered) {
  if (bark_enabled_ && delivered) {
    if (!bark_outbox_.empty()) {
      bark_outbox_.pop_front();
    }
  }
}

void DeliveryCoordinator::acknowledge_mqtt(bool delivered) {
  if (mqtt_enabled_ && delivered) {
    mqtt_outbox_.pop();
  }
}

}  // namespace aquarium
