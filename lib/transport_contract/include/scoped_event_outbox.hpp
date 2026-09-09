#pragma once

#include "transport_contract.hpp"

#include <cstddef>
#include <deque>

namespace aquarium::transport {

class ScopedEventOutbox {
 public:
  explicit ScopedEventOutbox(std::size_t capacity) : capacity_(capacity) {}

  bool push(const ScopedTemperatureEvent& event);
  const ScopedTemperatureEvent* front() const;
  void pop();
  bool empty() const { return events_.empty(); }
  std::size_t dropped_count() const { return dropped_count_; }

 private:
  std::size_t capacity_;
  std::size_t dropped_count_ = 0;
  std::deque<ScopedTemperatureEvent> events_;
};

}  // namespace aquarium::transport
