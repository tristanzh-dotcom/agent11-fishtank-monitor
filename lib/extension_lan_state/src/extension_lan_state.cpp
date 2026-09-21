#include "extension_lan_state.hpp"

#include <cmath>
#include <cstring>
#include <limits>

#if defined(ARDUINO)
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <mbedtls/md.h>
#else
#include <CommonCrypto/CommonHMAC.h>
#endif

namespace aquarium::extension_lan {
namespace {

constexpr std::size_t kSignedBytes = 40U;
constexpr std::size_t kTagOffset = kSignedBytes;

void put64(std::uint8_t* output, std::uint64_t value) {
  for (int index = 7; index >= 0; --index) {
    output[index] = static_cast<std::uint8_t>(value);
    value >>= 8U;
  }
}

void put32(std::uint8_t* output, std::uint32_t value) {
  for (int index = 3; index >= 0; --index) {
    output[index] = static_cast<std::uint8_t>(value);
    value >>= 8U;
  }
}

void put16(std::uint8_t* output, std::int16_t value) {
  const auto encoded = static_cast<std::uint16_t>(value);
  output[0] = static_cast<std::uint8_t>(encoded >> 8U);
  output[1] = static_cast<std::uint8_t>(encoded);
}

std::uint64_t get64(const std::uint8_t* input) {
  std::uint64_t result{};
  for (std::size_t index = 0U; index < 8U; ++index) {
    result = (result << 8U) | input[index];
  }
  return result;
}

std::uint32_t get32(const std::uint8_t* input) {
  return (static_cast<std::uint32_t>(input[0]) << 24U) |
         (static_cast<std::uint32_t>(input[1]) << 16U) |
         (static_cast<std::uint32_t>(input[2]) << 8U) | input[3];
}

std::int16_t get16(const std::uint8_t* input) {
  return static_cast<std::int16_t>(
      (static_cast<std::uint16_t>(input[0]) << 8U) | input[1]);
}

std::array<std::uint8_t, 32> hmac(const std::uint8_t* data, std::size_t size,
                                  const std::array<std::uint8_t, 32>& key) {
  std::array<std::uint8_t, 32> output{};
#if defined(ARDUINO)
  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  const auto* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == nullptr || mbedtls_md_setup(&context, info, 1) != 0 ||
      mbedtls_md_hmac_starts(&context, key.data(), key.size()) != 0 ||
      mbedtls_md_hmac_update(&context, data, size) != 0 ||
      mbedtls_md_hmac_finish(&context, output.data()) != 0) {
    mbedtls_md_free(&context);
    return {};
  }
  mbedtls_md_free(&context);
#else
  CCHmac(kCCHmacAlgSHA256, key.data(), key.size(), data, size, output.data());
#endif
  return output;
}

bool equalTag(const std::array<std::uint8_t, 32>& expected,
              const std::uint8_t* actual) {
  std::uint8_t difference{};
  for (std::size_t index = 0U; index < expected.size(); ++index) {
    difference |= expected[index] ^ actual[index];
  }
  return difference == 0U;
}

std::int16_t encodeTemperature(const std::optional<float>& value) {
  if (!value.has_value() || !std::isfinite(*value)) return kMissingTemperature;
  const auto scaled = std::lround(*value * 100.0F);
  return scaled < -5000 || scaled > 10000
             ? kMissingTemperature
             : static_cast<std::int16_t>(scaled);
}

bool validTemperature(std::int16_t value) {
  return value == kMissingTemperature || (value >= -5000 && value <= 10000);
}

}  // namespace

std::array<std::uint8_t, kPacketSize> encode(
    const State& state, std::uint64_t source_id, std::uint32_t sequence,
    const std::array<std::uint8_t, 32>& key) {
  std::array<std::uint8_t, kPacketSize> packet{};
  packet[0] = 'T';
  packet[1] = 'E';
  packet[2] = 'X';
  packet[3] = '1';
  packet[4] = 1U;
  put64(packet.data() + 8U, source_id);
  put32(packet.data() + 16U, sequence);
  put64(packet.data() + 20U, state.sampled_at_ms);
  for (std::size_t index = 0U; index < kSlotCount; ++index) {
    put16(packet.data() + 28U + index * 2U,
          encodeTemperature(state.temperature_c[index]));
    packet[32U + index] =
        static_cast<std::uint8_t>(state.thermal_state[index]);
  }
  const auto tag = hmac(packet.data(), kSignedBytes, key);
  std::memcpy(packet.data() + kTagOffset, tag.data(), tag.size());
  return packet;
}

bool accept(Reducer* reducer,
            const std::array<std::uint8_t, kPacketSize>& packet,
            std::uint64_t now_ms, const std::array<std::uint8_t, 32>& key) {
  if (reducer == nullptr || packet[0] != 'T' || packet[1] != 'E' ||
      packet[2] != 'X' || packet[3] != '1' || packet[4] != 1U ||
      !equalTag(hmac(packet.data(), kSignedBytes, key),
                packet.data() + kTagOffset)) {
    return false;
  }
  const auto source_id = get64(packet.data() + 8U);
  const auto sequence = get32(packet.data() + 16U);
  const auto sampled_at_ms = get64(packet.data() + 20U);
  if (reducer->has_packet && reducer->source_id == source_id &&
      sequence <= reducer->sequence) {
    return false;
  }
  State state{};
  state.sampled_at_ms = sampled_at_ms;
  for (std::size_t index = 0U; index < kSlotCount; ++index) {
    const auto temperature = get16(packet.data() + 28U + index * 2U);
    if (!validTemperature(temperature) || packet[32U + index] > 3U ||
        (temperature == kMissingTemperature && packet[32U + index] != 3U) ||
        (temperature != kMissingTemperature && packet[32U + index] == 3U)) {
      return false;
    }
    state.temperature_c[index] =
        temperature == kMissingTemperature
            ? std::optional<float>{}
            : std::optional<float>{static_cast<float>(temperature) / 100.0F};
    state.thermal_state[index] =
        static_cast<ThermalState>(packet[32U + index]);
  }
  reducer->has_packet = true;
  reducer->source_id = source_id;
  reducer->sequence = sequence;
  reducer->accepted_at_ms = now_ms;
  reducer->state = state;
  return true;
}

State freshState(const Reducer& reducer, std::uint64_t now_ms) {
  if (!reducer.has_packet || now_ms < reducer.accepted_at_ms) return {};
  State state = reducer.state;
  // Accept the legacy-compatible frame shape, but never expose data from the
  // reserved first slot to Agent11 consumers.
  state.temperature_c[kReservedNoSignalSlot].reset();
  state.thermal_state[kReservedNoSignalSlot] = ThermalState::no_signal;
  if (now_ms - reducer.accepted_at_ms > kFreshnessMs) {
    state.temperature_c = {};
    state.thermal_state = {ThermalState::no_signal, ThermalState::no_signal};
  }
  return state;
}

std::optional<TemperatureSample> nextTemperatureSample(
    const State& state, std::uint64_t at_ms,
    std::optional<std::uint64_t>* last_source_sampled_at_ms) {
  if (last_source_sampled_at_ms == nullptr) return std::nullopt;

  const auto& temperature = state.temperature_c[kPlecoSlot];
  if (temperature.has_value()) {
    if (last_source_sampled_at_ms->has_value() &&
        *last_source_sampled_at_ms == state.sampled_at_ms) {
      return std::nullopt;
    }
    *last_source_sampled_at_ms = state.sampled_at_ms;
    return TemperatureSample{at_ms, static_cast<double>(*temperature),
                             std::nullopt};
  }

  return TemperatureSample{at_ms, std::nullopt, std::nullopt};
}

#if defined(ARDUINO)
namespace {
WiFiUDP udp;
Reducer reducer;
std::array<std::uint8_t, 32> key{};
bool ready{};
constexpr std::uint16_t kPort = 35112U;
}

void begin() {
  Preferences preferences;
  if (!preferences.begin("extension_lan", true)) return;
  ready = preferences.getBytesLength("state_key") == key.size() &&
          preferences.getBytes("state_key", key.data(), key.size()) == key.size();
  preferences.end();
  if (ready) ready = udp.begin(kPort) == 1;
  Serial.println(ready ? "EXTENSION_LAN_READY" : "EXTENSION_LAN_KEY_UNAVAILABLE");
}

void tick(std::uint64_t now_ms) {
  if (!ready || WiFi.status() != WL_CONNECTED) return;
  const int packet_size = udp.parsePacket();
  if (packet_size != static_cast<int>(kPacketSize)) return;
  std::array<std::uint8_t, kPacketSize> packet{};
  if (udp.read(packet.data(), packet.size()) ==
          static_cast<int>(packet.size()) &&
      accept(&reducer, packet, now_ms, key)) {
    Serial.println("EXTENSION_LAN_PACKET_ACCEPTED");
  }
  while (udp.available() > 0) udp.read();
}

State snapshot(std::uint64_t now_ms) { return freshState(reducer, now_ms); }
#else
void begin() {}
void tick(std::uint64_t) {}
State snapshot(std::uint64_t) { return {}; }
#endif

}  // namespace aquarium::extension_lan
