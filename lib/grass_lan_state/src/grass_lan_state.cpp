#include "grass_lan_state.hpp"

#include <cmath>
#include <cstring>

#if defined(ARDUINO)
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#endif

#if defined(ARDUINO)
#include <mbedtls/md.h>
#else
#include <CommonCrypto/CommonHMAC.h>
#endif

namespace aquarium::grass_lan {
namespace {

std::int16_t encodeTemperature(const std::optional<float>& value) {
  if (!value.has_value() || !std::isfinite(*value)) return -32768;
  const auto scaled = std::lround(*value * 100.0F);
  return scaled < -5000 || scaled > 10000
             ? static_cast<std::int16_t>(-32768)
             : static_cast<std::int16_t>(scaled);
}

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

std::array<std::uint8_t, 32> sign(const std::uint8_t* data, std::size_t size,
                                  const std::array<std::uint8_t, 32>& key) {
  std::array<std::uint8_t, 32> tag{};
#if defined(ARDUINO)
  mbedtls_md_context_t context;
  mbedtls_md_init(&context);
  const auto* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == nullptr || mbedtls_md_setup(&context, info, 1) != 0 ||
      mbedtls_md_hmac_starts(&context, key.data(), key.size()) != 0 ||
      mbedtls_md_hmac_update(&context, data, size) != 0 ||
      mbedtls_md_hmac_finish(&context, tag.data()) != 0) {
    tag.fill(0U);
  }
  mbedtls_md_free(&context);
#else
  CCHmac(kCCHmacAlgSHA256, key.data(), key.size(), data, size, tag.data());
#endif
  return tag;
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

bool equalTag(const std::array<std::uint8_t, 32>& expected,
              const std::uint8_t* actual) {
  std::uint8_t difference{};
  for (std::size_t index = 0U; index < expected.size(); ++index) {
    difference |= expected[index] ^ actual[index];
  }
  return difference == 0U;
}

}  // namespace

std::array<std::uint8_t, kPacketSize> encode(
    const State& snapshot, std::uint64_t source_id, std::uint32_t sequence,
    const std::array<std::uint8_t, 32>& key) {
  std::array<std::uint8_t, kPacketSize> packet{};
  packet[0] = 'T';
  packet[1] = 'G';
  packet[2] = 'R';
  packet[3] = '1';
  packet[4] = 1U;
  put64(packet.data() + 8U, source_id);
  put32(packet.data() + 16U, sequence);
  put64(packet.data() + 20U, snapshot.sampled_at_ms);
  for (std::size_t index = 0U; index < 2U; ++index) {
    put16(packet.data() + 28U + index * 2U,
          encodeTemperature(snapshot.temperature_c[index]));
    packet[32U + index] =
        static_cast<std::uint8_t>(snapshot.thermal_state[index]);
  }
  const auto tag = sign(packet.data(), 40U, key);
  std::memcpy(packet.data() + 40U, tag.data(), tag.size());
  return packet;
}

bool decode(const std::array<std::uint8_t, kPacketSize>& packet,
            const std::array<std::uint8_t, 32>& key, DecodedState* output) {
  if (output == nullptr || packet[0] != 'T' || packet[1] != 'G' ||
      packet[2] != 'R' || packet[3] != '1' || packet[4] != 1U ||
      !equalTag(sign(packet.data(), 40U, key), packet.data() + 40U)) {
    return false;
  }
  DecodedState decoded{};
  decoded.source_id = get64(packet.data() + 8U);
  decoded.sequence = get32(packet.data() + 16U);
  decoded.snapshot.sampled_at_ms = get64(packet.data() + 20U);
  for (std::size_t index = 0U; index < 2U; ++index) {
    const auto encoded = get16(packet.data() + 28U + index * 2U);
    const auto state = packet[32U + index];
    if (state > static_cast<std::uint8_t>(ThermalState::no_signal) ||
        (encoded != -32768 && (encoded < -5000 || encoded > 10000)) ||
        (encoded == -32768 && state !=
                                  static_cast<std::uint8_t>(ThermalState::no_signal)) ||
        (encoded != -32768 && state ==
                                  static_cast<std::uint8_t>(ThermalState::no_signal))) {
      return false;
    }
    decoded.snapshot.temperature_c[index] =
        encoded == -32768
            ? std::optional<float>{}
            : std::optional<float>{static_cast<float>(encoded) / 100.0F};
    decoded.snapshot.thermal_state[index] =
        static_cast<ThermalState>(state);
  }
  *output = decoded;
  return true;
}

bool accept(Reducer* reducer,
            const std::array<std::uint8_t, kPacketSize>& packet,
            std::uint64_t now_ms, const std::array<std::uint8_t, 32>& key) {
  if (reducer == nullptr) return false;
  DecodedState decoded{};
  if (!decode(packet, key, &decoded) || decoded.source_id == 0U ||
      decoded.snapshot.temperature_c[1].has_value() ||
      decoded.snapshot.thermal_state[1] != ThermalState::no_signal ||
      decoded.source_id == reducer->retired_source_id) {
    return false;
  }
  if (reducer->has_packet && decoded.source_id == reducer->source_id) {
    if (decoded.sequence <= reducer->sequence ||
        decoded.snapshot.sampled_at_ms <= reducer->snapshot.sampled_at_ms) {
      return false;
    }
  } else if (reducer->has_packet) {
    reducer->retired_source_id = reducer->source_id;
  }
  reducer->source_id = decoded.source_id;
  reducer->sequence = decoded.sequence;
  reducer->accepted_at_ms = now_ms;
  reducer->snapshot = decoded.snapshot;
  reducer->has_packet = true;
  return true;
}

State freshState(const Reducer& reducer, std::uint64_t now_ms) {
  if (!reducer.has_packet || now_ms < reducer.accepted_at_ms) return {};
  State result = reducer.snapshot;
  result.has_packet = true;
  result.fresh = now_ms - reducer.accepted_at_ms <= 75000U;
  return result;
}

ConnectivityEvent observeConnectivity(ConnectivityTracker* tracker,
                                      const State& state) {
  if (tracker == nullptr || !state.has_packet) return ConnectivityEvent::none;
  if (!tracker->initialized) {
    tracker->initialized = true;
    tracker->offline = !state.fresh;
    return tracker->offline ? ConnectivityEvent::offline : ConnectivityEvent::none;
  }
  if (!tracker->offline && !state.fresh) {
    tracker->offline = true;
    return ConnectivityEvent::offline;
  }
  if (tracker->offline && state.fresh) {
    tracker->offline = false;
    return ConnectivityEvent::recovered;
  }
  return ConnectivityEvent::none;
}

#if defined(ARDUINO)
namespace {
WiFiUDP grass_udp;
Reducer grass_reducer;
std::array<std::uint8_t, 32> grass_key{};
bool grass_ready{};
}  // namespace

void begin() {
  Preferences preferences;
  if (!preferences.begin("grass_lan", true)) {
    Serial.println("GRASS_LAN_KEY_UNAVAILABLE");
    return;
  }
  grass_ready = preferences.getBytesLength("state_key") == grass_key.size() &&
      preferences.getBytes("state_key", grass_key.data(), grass_key.size()) ==
          grass_key.size();
  preferences.end();
  if (grass_ready) grass_ready = grass_udp.begin(35115U) == 1;
  Serial.println(grass_ready ? "GRASS_LAN_READY" : "GRASS_LAN_KEY_UNAVAILABLE");
}

void tick(std::uint64_t now_ms) {
  if (!grass_ready || WiFi.status() != WL_CONNECTED) return;
  for (int handled = 0; handled < 4; ++handled) {
    const int packet_size = grass_udp.parsePacket();
    if (packet_size <= 0) break;
    std::array<std::uint8_t, kPacketSize> packet{};
    if (packet_size == static_cast<int>(packet.size()) &&
        grass_udp.read(packet.data(), packet.size()) ==
            static_cast<int>(packet.size()) &&
        accept(&grass_reducer, packet, now_ms, grass_key)) {
      Serial.println("GRASS_LAN_PACKET_ACCEPTED");
    }
    while (grass_udp.available() > 0) grass_udp.read();
  }
}

State snapshot(std::uint64_t now_ms) { return freshState(grass_reducer, now_ms); }
#else
void begin() {}
void tick(std::uint64_t) {}
State snapshot(std::uint64_t) { return {}; }
#endif

}  // namespace aquarium::grass_lan
