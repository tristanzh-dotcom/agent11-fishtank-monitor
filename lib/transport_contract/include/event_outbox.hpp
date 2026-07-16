#pragma once

#include "temperature_engine.hpp"

#include <cstddef>
#include <deque>

namespace aquarium {

class EventOutbox {
 public:
  explicit EventOutbox(std::size_t capacity) : capacity_(capacity) {}

  bool push(const TemperatureEvent& event);
  const TemperatureEvent* front() const;
  void pop();
  bool empty() const { return events_.empty(); }
  std::size_t dropped_count() const { return dropped_count_; }

 private:
  std::size_t capacity_;
  std::size_t dropped_count_ = 0;
  std::deque<TemperatureEvent> events_;
};

}  // namespace aquarium
