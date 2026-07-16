#pragma once

#include "event_outbox.hpp"

#include <cstddef>

namespace aquarium {

class DeliveryCoordinator {
 public:
  DeliveryCoordinator(std::size_t capacity, bool bark_enabled, bool mqtt_enabled)
      : bark_enabled_(bark_enabled),
        mqtt_enabled_(mqtt_enabled),
        bark_outbox_(capacity),
        mqtt_outbox_(capacity) {}

  bool enqueue(const TemperatureEvent& event);
  const TemperatureEvent* bark_front() const;
  const TemperatureEvent* mqtt_front() const;
  void acknowledge_bark(bool delivered);
  void acknowledge_mqtt(bool delivered);

 private:
  bool bark_enabled_;
  bool mqtt_enabled_;
  EventOutbox bark_outbox_;
  EventOutbox mqtt_outbox_;
};

}  // namespace aquarium
