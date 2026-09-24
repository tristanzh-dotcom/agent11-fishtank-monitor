#pragma once
constexpr int WL_CONNECTED = 3;
inline struct TestWiFi {
  int connection = WL_CONNECTED;
  int status() const { return connection; }
} WiFi;
inline struct TestSerial {
  void println(const char*) {}
} Serial;
