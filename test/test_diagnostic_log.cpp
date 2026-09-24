#include "diagnostic_log.hpp"

#include <cassert>
#include <cstdint>

int main() {
  using namespace aquarium::firmware::diagnostics;

  LegacyLog old{};
  old.magic = kMagic;
  old.version = kLegacyVersion;
  old.next = 2U;
  old.count = 2U;
  old.overwritten = 7U;
  old.records[0] = {100U, 1700000000U, 75000U, 249U,
                    Event::grass_stale};
  old.records[1] = {115U, 1700000015U, 90000U, 2U,
                    Event::grass_recovered};

  Log current{};
  assert(migrateLegacy(old, &current));
  assert(valid(current));
  assert(current.count == 2U && current.overwritten == 7U);
  assert(current.records[0].event == Event::grass_stale);
  assert(current.records[1].event == Event::grass_recovered);
  assert((current.records[0].receive.flags & kHasReceiveSnapshot) == 0U);

  Record stale{};
  stale.event = Event::grass_stale;
  setReceiveSnapshot(&stale, 70000U, 69999U, 2U);
  assert(stale.receive.received == UINT16_MAX);
  assert(stale.receive.accepted == UINT16_MAX);
  assert(stale.receive.rejected == 2U);
  assert((stale.receive.flags & kHasReceiveSnapshot) != 0U);
  assert((stale.receive.flags & kReceiveSnapshotSaturated) != 0U);

  append(&current, stale);
  Log after_power_cycle = current;
  assert(valid(after_power_cycle));
  assert(after_power_cycle.records[after_power_cycle.next == 0U
                                       ? kCapacity - 1U
                                       : after_power_cycle.next - 1U]
             .receive.rejected == 2U);

  for (std::uint32_t i = 0U; i < kCapacity + 3U; ++i) {
    Record item{};
    item.uptime_ms = i;
    item.event = Event::grass_rejected;
    append(&after_power_cycle, item);
  }
  assert(after_power_cycle.count == kCapacity);
  assert(after_power_cycle.overwritten == 13U);
}
