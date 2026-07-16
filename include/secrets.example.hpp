#pragma once

// Copy this file to include/secrets.hpp.  That local file is intentionally
// ignored by Git. Do not paste credentials into source files or serial logs.

namespace aquarium::secrets {

constexpr char kWifiSsid[] = "replace-with-wifi-ssid";
constexpr char kWifiPassword[] = "replace-with-wifi-password";

// Optional MQTT endpoint and credentials. MQTT is disabled by default in
// config.hpp and is not required for the direct-Bark MVP.
constexpr char kAliyunHost[] = "replace-with-product-key.iot-as-mqtt.cn-shanghai.aliyuncs.com";
constexpr unsigned short kAliyunPort = 8883;
constexpr char kAliyunClientId[] = "replace-with-mqtt-client-id";
constexpr char kAliyunUsername[] = "replace-with-mqtt-username";
constexpr char kAliyunPassword[] = "replace-with-mqtt-password";
constexpr char kAliyunProductKey[] = "replace-with-product-key";
constexpr char kAliyunDeviceName[] = "tank01";

// PEM root certificate for the Alibaba endpoint. Keep the line breaks as \n.
constexpr char kAliyunRootCaPem[] = "";

// Direct Bark is the MVP's primary event path. Keep the real key and CA only in
// the ignored include/secrets.hpp file.
constexpr char kBarkDeviceKey[] = "";
constexpr char kBarkRootCaPem[] = "";

}  // namespace aquarium::secrets
