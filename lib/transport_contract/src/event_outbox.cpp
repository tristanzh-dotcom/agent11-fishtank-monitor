#include "event_outbox.hpp"

namespace aquarium {

bool EventOutbox::push(const TemperatureEvent& event) {
  if (events_.size() >= capacity_) {
    ++dropped_count_;
    return false;
  }
  events_.push_back(event);
  return true;
}

const TemperatureEvent* EventOutbox::front() const {
  return events_.empty() ? nullptr : &events_.front();
}

void EventOutbox::pop() {
  if (!events_.empty()) {
    events_.pop_front();
  }
}

}  // namespace aquarium
