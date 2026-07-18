#include "heartbeat_notifier.hpp"

#include "heartbeat_contract.hpp"
#include "secrets.hpp"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <esp_random.h>
#include <mbedtls/md.h>

#include <array>
#include <cstdio>
#include <ctime>
#include <string>

namespace aquarium::firmware {
namespace {

constexpr std::time_t kMinimumReasonableEpochSeconds = 1700000000;

std::string hex_encode(const unsigned char* bytes, std::size_t size) {
  constexpr char kHex[] = "0123456789abcdef";
  std::string encoded(size * 2U, '0');
  for (std::size_t index = 0; index < size; ++index) {
    encoded[index * 2U] = kHex[(bytes[index] >> 4U) & 0x0fU];
    encoded[index * 2U + 1U] = kHex[bytes[index] & 0x0fU];
  }
  return encoded;
}

std::string random_nonce() {
  std::array<unsigned char, 16> bytes{};
  for (std::size_t offset = 0; offset < bytes.size();
       offset += sizeof(std::uint32_t)) {
    const std::uint32_t random = esp_random();
    bytes[offset] = static_cast<unsigned char>(random >> 24U);
    bytes[offset + 1U] = static_cast<unsigned char>(random >> 16U);
    bytes[offset + 2U] = static_cast<unsigned char>(random >> 8U);
    bytes[offset + 3U] = static_cast<unsigned char>(random);
  }
  return hex_encode(bytes.data(), bytes.size());
}

bool sha256_hex(const std::string& value, std::string& digest_hex) {
  const auto* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == nullptr) {
    return false;
  }
  std::array<unsigned char, 32> digest{};
  if (mbedtls_md(info, reinterpret_cast<const unsigned char*>(value.data()),
                 value.size(), digest.data()) != 0) {
    return false;
  }
  digest_hex = hex_encode(digest.data(), digest.size());
  return true;
}

bool hmac_sha256_hex(const std::string& key, const std::string& value,
                     std::string& digest_hex) {
  const auto* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (info == nullptr) {
    return false;
  }
  std::array<unsigned char, 32> digest{};
  if (mbedtls_md_hmac(
          info, reinterpret_cast<const unsigned char*>(key.data()), key.size(),
          reinterpret_cast<const unsigned char*>(value.data()), value.size(),
          digest.data()) != 0) {
    return false;
  }
  digest_hex = hex_encode(digest.data(), digest.size());
  return true;
}

}  // namespace

void HeartbeatNotifier::begin_time_sync() {
  configTime(0, 0, "pool.ntp.org", "time.cloudflare.com");
}

bool HeartbeatNotifier::notify(double main_c, double sump_c,
                               std::uint64_t uptime_ms) {
  if (secrets::kTencentFunctionUrl[0] == '\0' ||
      secrets::kTencentDeviceSecret[0] == '\0' ||
      secrets::kTencentRootCaPem[0] == '\0') {
    return false;
  }

  const std::time_t current_time = std::time(nullptr);
  if (current_time < kMinimumReasonableEpochSeconds) {
    return false;
  }

  const heartbeat::HeartbeatPayload payload{
      "tank01",
      static_cast<std::uint64_t>(current_time) * 1000ULL,
      random_nonce(),
      main_c,
      sump_c,
      uptime_ms,
  };
  const std::string body = heartbeat::heartbeat_json(payload);

  std::string body_hash;
  if (!sha256_hex(body, body_hash)) {
    return false;
  }
  const std::string canonical =
      heartbeat::signature_canonical(payload, body_hash);
  std::string signature;
  if (!hmac_sha256_hex(secrets::kTencentDeviceSecret, canonical, signature)) {
    return false;
  }

  WiFiClientSecure client;
  client.setCACert(secrets::kTencentRootCaPem);
  client.setTimeout(5000);
  HTTPClient http;
  http.setConnectTimeout(5000);
  if (!http.begin(client, secrets::kTencentFunctionUrl)) {
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-Aquarium-Signature", signature.c_str());
  const int status = http.POST(body.c_str());
  http.end();
  return status >= 200 && status < 300;
}

}  // namespace aquarium::firmware

