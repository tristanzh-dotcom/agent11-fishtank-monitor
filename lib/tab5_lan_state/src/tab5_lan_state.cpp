#include "tab5_lan_state.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace aquarium::tab5 {
namespace {

constexpr std::size_t kSignedBytes = 40U;
constexpr std::size_t kTagOffset = kSignedBytes;

std::uint32_t rotate_right(std::uint32_t value, std::uint32_t bits) {
  return (value >> bits) | (value << (32U - bits));
}

std::array<std::uint8_t, 32> sha256(const std::uint8_t* input,
                                    std::size_t size) {
  constexpr std::array<std::uint32_t, 64> constants{
      0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
      0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
      0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
      0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
      0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
      0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
      0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
      0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
      0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
      0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
      0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
      0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
      0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
      0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
      0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
      0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U};
  const std::size_t padded = ((size + 9U + 63U) / 64U) * 64U;
  std::array<std::uint8_t, 128> buffer{};
  if (padded > buffer.size()) return {};
  std::memcpy(buffer.data(), input, size);
  buffer[size] = 0x80U;
  const auto bit_count = static_cast<std::uint64_t>(size) * 8U;
  for (std::size_t index = 0U; index < 8U; ++index) {
    buffer[padded - 1U - index] =
        static_cast<std::uint8_t>(bit_count >> (index * 8U));
  }

  std::array<std::uint32_t, 8> hash{
      0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
      0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U};
  for (std::size_t offset = 0U; offset < padded; offset += 64U) {
    std::array<std::uint32_t, 64> words{};
    for (std::size_t index = 0U; index < 16U; ++index) {
      words[index] =
          (static_cast<std::uint32_t>(buffer[offset + index * 4U]) << 24U) |
          (static_cast<std::uint32_t>(buffer[offset + index * 4U + 1U])
           << 16U) |
          (static_cast<std::uint32_t>(buffer[offset + index * 4U + 2U])
           << 8U) |
          buffer[offset + index * 4U + 3U];
    }
    for (std::size_t index = 16U; index < words.size(); ++index) {
      const auto s0 = rotate_right(words[index - 15U], 7U) ^
                      rotate_right(words[index - 15U], 18U) ^
                      (words[index - 15U] >> 3U);
      const auto s1 = rotate_right(words[index - 2U], 17U) ^
                      rotate_right(words[index - 2U], 19U) ^
                      (words[index - 2U] >> 10U);
      words[index] = words[index - 16U] + s0 + words[index - 7U] + s1;
    }

    auto a = hash[0];
    auto b = hash[1];
    auto c = hash[2];
    auto d = hash[3];
    auto e = hash[4];
    auto f = hash[5];
    auto g = hash[6];
    auto h = hash[7];
    for (std::size_t index = 0U; index < words.size(); ++index) {
      const auto s1 = rotate_right(e, 6U) ^ rotate_right(e, 11U) ^
                      rotate_right(e, 25U);
      const auto choice = (e & f) ^ ((~e) & g);
      const auto t1 = h + s1 + choice + constants[index] + words[index];
      const auto s0 = rotate_right(a, 2U) ^ rotate_right(a, 13U) ^
                      rotate_right(a, 22U);
      const auto majority = (a & b) ^ (a & c) ^ (b & c);
      const auto t2 = s0 + majority;
      h = g;
      g = f;
      f = e;
      e = d + t1;
      d = c;
      c = b;
      b = a;
      a = t1 + t2;
    }
    hash[0] += a;
    hash[1] += b;
    hash[2] += c;
    hash[3] += d;
    hash[4] += e;
    hash[5] += f;
    hash[6] += g;
    hash[7] += h;
  }

  std::array<std::uint8_t, 32> output{};
  for (std::size_t index = 0U; index < hash.size(); ++index) {
    for (std::size_t byte = 0U; byte < 4U; ++byte) {
      output[index * 4U + byte] =
          static_cast<std::uint8_t>(hash[index] >> (24U - byte * 8U));
    }
  }
  return output;
}

std::array<std::uint8_t, 32> hmac_sha256(
    const std::uint8_t* input, std::size_t size,
    const std::array<std::uint8_t, 32>& key) {
  std::array<std::uint8_t, 128> inner{};
  std::array<std::uint8_t, 96> outer{};
  for (std::size_t index = 0U; index < 64U; ++index) {
    const auto byte = index < key.size() ? key[index] : 0U;
    inner[index] = static_cast<std::uint8_t>(byte ^ 0x36U);
    outer[index] = static_cast<std::uint8_t>(byte ^ 0x5cU);
  }
  std::memcpy(inner.data() + 64U, input, size);
  const auto inner_hash = sha256(inner.data(), 64U + size);
  std::memcpy(outer.data() + 64U, inner_hash.data(), inner_hash.size());
  return sha256(outer.data(), outer.size());
}

std::int16_t centi(const std::optional<double>& value) {
  if (!value.has_value() || !std::isfinite(*value)) {
    return kMissingTemperature;
  }
  const auto scaled = std::lround(*value * 100.0);
  return scaled < -5000 || scaled > 10000
             ? kMissingTemperature
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
  const auto unsigned_value = static_cast<std::uint16_t>(value);
  output[0] = static_cast<std::uint8_t>(unsigned_value >> 8U);
  output[1] = static_cast<std::uint8_t>(unsigned_value);
}

}  // namespace

LanState make_lan_state(
    const TemperatureSample& sample,
    const std::array<std::optional<double>, 3>& auxiliary_c,
    const std::array<ThermalState, 3>& auxiliary_thermal_state,
    const ActiveEventSnapshot& active) {
  LanState output{};
  output.main_centi_c = centi(sample.display_c);
  output.sump_centi_c = centi(sample.return_c);
  output.thermal_state = output.main_centi_c == kMissingTemperature
                             ? ThermalState::no_signal
                             : ThermalState::normal;
  for (std::size_t index = 0U; index < auxiliary_c.size(); ++index) {
    output.auxiliary_centi_c[index] = centi(auxiliary_c[index]);
    output.auxiliary_thermal_state[index] =
        output.auxiliary_centi_c[index] == kMissingTemperature
            ? ThermalState::no_signal
            : auxiliary_thermal_state[index];
  }
  for (const auto& event : active.events()) {
    output.severity = std::max(
        output.severity,
        static_cast<std::uint8_t>(event.severity == Severity::n3 ? 3U : 2U));
    if (event.type == EventType::sensor_fault) {
      output.sensor_fault = true;
    } else if (event.type == EventType::high_temperature ||
               event.type == EventType::high_temperature_critical) {
      output.thermal_state = ThermalState::high;
    } else if (event.type == EventType::low_temperature ||
               event.type == EventType::low_temperature_critical) {
      output.thermal_state = ThermalState::low;
    }
  }
  if (output.sensor_fault || output.main_centi_c == kMissingTemperature) {
    output.thermal_state = ThermalState::no_signal;
  }
  return output;
}

std::array<std::uint8_t, kPacketSize> encode_packet(
    const LanState& state, std::uint64_t source_id, std::uint32_t sequence,
    const std::array<std::uint8_t, 32>& key) {
  std::array<std::uint8_t, kPacketSize> packet{};
  packet[0] = 'T';
  packet[1] = '5';
  packet[2] = 'L';
  packet[3] = '1';
  packet[4] = 2U;
  put64(packet.data() + 8U, source_id);
  put32(packet.data() + 16U, sequence);
  put16(packet.data() + 20U, state.main_centi_c);
  put16(packet.data() + 22U, state.sump_centi_c);
  packet[24] = static_cast<std::uint8_t>(state.thermal_state);
  packet[25] = state.severity;
  packet[26] = state.sensor_fault ? 1U : 0U;
  for (std::size_t index = 0U; index < state.auxiliary_centi_c.size(); ++index) {
    put16(packet.data() + 28U + index * 2U, state.auxiliary_centi_c[index]);
    packet[34U + index] =
        static_cast<std::uint8_t>(state.auxiliary_thermal_state[index]);
  }
  const auto tag = hmac_sha256(packet.data(), kSignedBytes, key);
  std::memcpy(packet.data() + kTagOffset, tag.data(), tag.size());
  return packet;
}

}  // namespace aquarium::tab5
