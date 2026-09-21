#include "scoped_event_outbox.hpp"

namespace aquarium::transport {

bool ScopedEventOutbox::push(const ScopedTemperatureEvent& event) {
  if (event.event.state == EventState::resolved &&
      events_.size() >= capacity_) {
    remove_pending(event.tank_key, event.event.type);
  }
  if (events_.size() >= capacity_) {
    ++dropped_count_;
    return false;
  }
  const auto prepared = bark_policy_.prepare(event.event, event.tank_key);
  if (!prepared.has_value()) return false;
  if (prepared->state == EventState::resolved ||
      prepared->state == EventState::escalated) {
    remove_pending(event.tank_key, prepared->type);
  }
  auto queued = event;
  queued.event = *prepared;
  events_.push_back(queued);
  return true;
}

void ScopedEventOutbox::remove_pending(const std::string& tank_key,
                                       EventType type) {
  const auto problem = BarkAlertPolicy::problem_type(type);
  for (auto it = events_.begin(); it != events_.end();) {
    if (it->tank_key == tank_key &&
        BarkAlertPolicy::problem_type(it->event.type) == problem) {
      it = events_.erase(it);
    } else {
      ++it;
    }
  }
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
