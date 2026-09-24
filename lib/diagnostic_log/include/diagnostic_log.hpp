#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace aquarium::firmware::diagnostics {

constexpr std::size_t kCapacity = 32U;
constexpr std::uint32_t kMagic = 0x44494147U;
constexpr std::uint32_t kLegacyVersion = 1U;
constexpr std::uint32_t kVersion = 2U;
constexpr std::uint8_t kHasReceiveSnapshot = 1U;
constexpr std::uint8_t kReceiveSnapshotSaturated = 2U;

enum class Event : std::uint8_t {
  boot,
  wifi_down,
  wifi_up,
  loop_gap,
  extension_first_packet,
  extension_sender_session,
  extension_stale,
  extension_recovered,
  extension_receive_gap,
  grass_first_packet,
  grass_sender_session,
  grass_receive_gap,
  grass_stale,
  grass_recovered,
  grass_rejected,
};

struct LegacyRecord {
  std::uint32_t uptime_ms{};
  std::uint32_t epoch_seconds{};
  std::uint32_t value{};
  std::uint32_t detail{};
  Event event{};
};

struct LegacyLog {
  std::uint32_t magic{};
  std::uint32_t version{};
  std::uint32_t next{};
  std::uint32_t count{};
  std::uint32_t overwritten{};
  std::array<LegacyRecord, kCapacity> records{};
};

struct ReceiveSnapshot {
  std::uint16_t received{};
  std::uint16_t accepted{};
  std::uint16_t rejected{};
  std::uint8_t flags{};
};

struct Record {
  std::uint32_t uptime_ms{};
  std::uint32_t epoch_seconds{};
  std::uint32_t value{};
  std::uint32_t detail{};
  Event event{};
  ReceiveSnapshot receive{};
};

struct Log {
  std::uint32_t magic{};
  std::uint32_t version{};
  std::uint32_t next{};
  std::uint32_t count{};
  std::uint32_t overwritten{};
  std::array<Record, kCapacity> records{};
};

static_assert(sizeof(LegacyRecord) == 20U, "legacy NVS layout changed");
static_assert(sizeof(LegacyLog) == 660U, "legacy NVS blob size changed");
static_assert(sizeof(Log) <= 1024U, "diagnostic NVS blob grew unexpectedly");

bool valid(const Log& log);
bool migrateLegacy(const LegacyLog& legacy, Log* destination);
void append(Log* log, const Record& record);
void setReceiveSnapshot(Record* record, std::uint32_t received,
                        std::uint32_t accepted, std::uint32_t rejected);

}  // namespace aquarium::firmware::diagnostics
