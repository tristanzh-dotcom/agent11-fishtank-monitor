#include "wifi_power_policy.hpp"

#include <cassert>

namespace {

enum class SleepMode { none, modem };

struct WifiStub {
  SleepMode sleep_mode{SleepMode::modem};
  bool accept_setting{true};
  bool requested_enabled{true};

  bool setSleep(bool enabled) {
    requested_enabled = enabled;
    if (!accept_setting) return false;
    sleep_mode = enabled ? SleepMode::modem : SleepMode::none;
    return true;
  }

  SleepMode getSleep() const { return sleep_mode; }
};

void disables_modem_sleep_and_verifies_the_applied_mode() {
  WifiStub wifi;

  const bool configured =
      aquarium::firmware::configureNoWifiPowerSave(wifi, SleepMode::none);

  assert(configured);
  assert(!wifi.requested_enabled);
  assert(wifi.sleep_mode == SleepMode::none);
}

void reports_a_failed_wifi_power_policy_change() {
  WifiStub wifi;
  wifi.accept_setting = false;

  const bool configured =
      aquarium::firmware::configureNoWifiPowerSave(wifi, SleepMode::none);

  assert(!configured);
  assert(!wifi.requested_enabled);
  assert(wifi.sleep_mode == SleepMode::modem);
}

}  // namespace

int main() {
  disables_modem_sleep_and_verifies_the_applied_mode();
  reports_a_failed_wifi_power_policy_change();
}
