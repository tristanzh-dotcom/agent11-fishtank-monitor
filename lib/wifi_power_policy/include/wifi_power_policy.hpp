#pragma once

namespace aquarium::firmware {

template <typename Wifi, typename NoSleepMode>
bool configureNoWifiPowerSave(Wifi& wifi, NoSleepMode no_sleep_mode) {
  return wifi.setSleep(false) && wifi.getSleep() == no_sleep_mode;
}

}  // namespace aquarium::firmware
