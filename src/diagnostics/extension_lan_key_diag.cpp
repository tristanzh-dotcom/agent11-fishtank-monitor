#include <Arduino.h>
#include <Preferences.h>

#include <array>
#include <cctype>
#include <cstdint>

namespace {
constexpr char kNamespace[] = "extension_lan";
constexpr char kKeyName[] = "state_key";
constexpr char kPrefix[] = "EXTENSION_LAN_KEY ";
constexpr char kGrassPrefix[] = "GRASS_LAN_KEY ";

int hexValue(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}

bool decodeKey(const String& input, std::array<std::uint8_t, 32>* output) {
  if (output == nullptr || input.length() != 64U) return false;
  for (std::size_t index = 0; index < output->size(); ++index) {
    const int high = hexValue(input[index * 2U]);
    const int low = hexValue(input[index * 2U + 1U]);
    if (high < 0 || low < 0) return false;
    (*output)[index] = static_cast<std::uint8_t>((high << 4U) | low);
  }
  return true;
}

void writeKey(const String& line) {
  const bool grass = line.startsWith(kGrassPrefix);
  if (!grass && !line.startsWith(kPrefix)) return;
  std::array<std::uint8_t, 32> key{};
  if (!decodeKey(line.substring(grass ? sizeof(kGrassPrefix) - 1U :
                                 sizeof(kPrefix) - 1U), &key)) {
    Serial.println(grass ? "GRASS_LAN_KEY_REJECTED" :
                           "EXTENSION_LAN_KEY_REJECTED");
    return;
  }
  Preferences preferences;
  const bool opened = preferences.begin(grass ? "grass_lan" : kNamespace, false);
  const bool stored = opened &&
      preferences.putBytes(kKeyName, key.data(), key.size()) == key.size();
  if (opened) preferences.end();
  key.fill(0U);
  Serial.println(grass ? (stored ? "GRASS_LAN_KEY_STORED" :
                                  "GRASS_LAN_KEY_STORE_FAILED")
                       : (stored ? "EXTENSION_LAN_KEY_STORED" :
                                   "EXTENSION_LAN_KEY_STORE_FAILED"));
}
}  // namespace

void setup() {
  Serial.begin(115200);
  Serial.println("EXTENSION_LAN_KEY_DIAG_READY");
}

void loop() {
  if (Serial.available() <= 0) return;
  writeKey(Serial.readStringUntil('\n'));
}
