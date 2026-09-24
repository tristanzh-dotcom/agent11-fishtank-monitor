#include "diagnostic_log.hpp"

#include <algorithm>
#include <limits>

namespace aquarium::firmware::diagnostics {
namespace {

std::uint16_t saturated(std::uint32_t value, bool* did_saturate) {
  if (value > std::numeric_limits<std::uint16_t>::max()) {
    *did_saturate = true;
    return std::numeric_limits<std::uint16_t>::max();
  }
  return static_cast<std::uint16_t>(value);
}

}  // namespace

bool valid(const Log& log) {
  return log.magic == kMagic && log.version == kVersion &&
         log.next < kCapacity && log.count <= kCapacity;
}

bool migrateLegacy(const LegacyLog& legacy, Log* destination) {
  if (destination == nullptr || legacy.magic != kMagic ||
      legacy.version != kLegacyVersion || legacy.next >= kCapacity ||
      legacy.count > kCapacity) {
    return false;
  }
  *destination = {};
  destination->magic = kMagic;
  destination->version = kVersion;
  destination->next = legacy.next;
  destination->count = legacy.count;
  destination->overwritten = legacy.overwritten;
  for (std::size_t i = 0U; i < kCapacity; ++i) {
    const auto& source = legacy.records[i];
    auto& target = destination->records[i];
    target.uptime_ms = source.uptime_ms;
    target.epoch_seconds = source.epoch_seconds;
    target.value = source.value;
    target.detail = source.detail;
    target.event = source.event;
  }
  return true;
}

void append(Log* log, const Record& record) {
  if (log == nullptr || !valid(*log)) return;
  log->records[log->next] = record;
  log->next = (log->next + 1U) % kCapacity;
  if (log->count < kCapacity) {
    ++log->count;
  } else {
    ++log->overwritten;
  }
}

void setReceiveSnapshot(Record* record, std::uint32_t received,
                        std::uint32_t accepted, std::uint32_t rejected) {
  if (record == nullptr) return;
  bool did_saturate = false;
  record->receive.received = saturated(received, &did_saturate);
  record->receive.accepted = saturated(accepted, &did_saturate);
  record->receive.rejected = saturated(rejected, &did_saturate);
  record->receive.flags = static_cast<std::uint8_t>(
      kHasReceiveSnapshot | (did_saturate ? kReceiveSnapshotSaturated : 0U));
}

}  // namespace aquarium::firmware::diagnostics
