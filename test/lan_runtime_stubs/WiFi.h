#pragma once
#include <cstdarg>
#include <cstdio>
#include <cstring>
constexpr int WL_CONNECTED = 3;
inline struct TestWiFi {
  int connection = WL_CONNECTED;
  int signal_dbm = -61;
  mutable unsigned rssi_queries{};
  int status() const { return connection; }
  int RSSI() const {
    ++rssi_queries;
    return signal_dbm;
  }
} WiFi;
inline struct TestSerial {
  unsigned rssi_markers{};
  char last_rssi_marker[128]{};
  void println(const char*) {}
  int printf(const char* format, ...) {
    char output[128]{};
    va_list args;
    va_start(args, format);
    const int length = std::vsnprintf(output, sizeof(output), format, args);
    va_end(args);
    if (std::strstr(format, "DIAG_WIFI_RSSI") != nullptr) {
      ++rssi_markers;
      std::snprintf(last_rssi_marker, sizeof(last_rssi_marker), "%s", output);
    }
    return length;
  }
} Serial;
