#pragma once

#include "event_outbox.hpp"
#include "transport_contract.hpp"

#include <cstddef>
#include <ctime>
#include <deque>
#include <optional>

namespace aquarium {

struct BarkDeliveryRecord {
  TemperatureEvent event;
  std::optional<std::time_t> event_time = std::nullopt;
};

class DeliveryCoordinator {
 public:
  DeliveryCoordinator(std::size_t capacity, bool bark_enabled, bool mqtt_enabled)
      : bark_enabled_(bark_enabled),
        mqtt_enabled_(mqtt_enabled),
        bark_capacity_(capacity),
        mqtt_outbox_(capacity) {}

  bool enqueue(const TemperatureEvent& event,
               std::optional<std::time_t> event_time = std::nullopt);
  const BarkDeliveryRecord* bark_record_front() const;
  const TemperatureEvent* bark_front() const;
  const TemperatureEvent* mqtt_front() const;
  void acknowledge_bark(bool delivered);
  void acknowledge_mqtt(bool delivered);

 private:
  void remove_pending(EventType type);

  bool bark_enabled_;
  bool mqtt_enabled_;
  std::size_t bark_capacity_;
  std::size_t bark_dropped_count_ = 0;
  std::deque<BarkDeliveryRecord> bark_outbox_;
  transport::BarkAlertPolicy bark_policy_;
  EventOutbox mqtt_outbox_;
};

}  // namespace aquarium
