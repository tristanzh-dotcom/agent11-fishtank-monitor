#include "scoped_event_outbox.hpp"

namespace aquarium::transport {

bool ScopedEventOutbox::push(const ScopedTemperatureEvent& event) {
  if (events_.size() >= capacity_) {
    ++dropped_count_;
    return false;
  }
  events_.push_back(event);
  return true;
}

const ScopedTemperatureEvent* ScopedEventOutbox::front() const {
  return events_.empty() ? nullptr : &events_.front();
}

void ScopedEventOutbox::pop() {
  if (!events_.empty()) {
    events_.pop_front();
  }
}

}  // namespace aquarium::transport
