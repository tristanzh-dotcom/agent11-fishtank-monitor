#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p .build
c++ -std=c++17 -Wall -Wextra -Werror -Ilib/wifi_power_policy/include \
  test/test_wifi_power_policy.cpp -o .build/wifi_power_policy_tests
.build/wifi_power_policy_tests
c++ -std=c++17 -Wall -Wextra -Werror -DARDUINO \
  -Itest/lan_runtime_stubs -Ilib/temperature_engine/include \
  -Ilib/extension_lan_state/include -Ilib/grass_lan_state/include \
  test/test_lan_runtime.cpp \
  lib/extension_lan_state/src/extension_lan_state.cpp \
  lib/grass_lan_state/src/grass_lan_state.cpp -o .build/lan_runtime_tests
if [ "$#" -eq 0 ]; then
  set -- malformed bounded connectivity late_packet burst_evidence grass_malformed wifi_unavailable
fi
for scenario do .build/lan_runtime_tests "$scenario"; done

python3 - <<'PY'
import os
from pathlib import Path
source = Path(os.environ.get('LAN_MAIN_SOURCE', 'src/main.cpp')).read_text()
start = source.index('  connect_wifi(now_ms);', source.index('void loop()'))
end = source.index('  if (now_ms - last_sample_at_ms < runtime_config.sample_interval_ms)', start)
Path('.build/lan_main_pass.inc').write_text(source[start:end])
PY
c++ -std=c++17 -Wall -Wextra -Werror -DARDUINO \
  -Itest/lan_runtime_stubs -I.build -Ilib/temperature_engine/include \
  -Ilib/extension_lan_state/include -Ilib/grass_lan_state/include \
  -Ilib/transport_contract/include test/test_lan_main_pass.cpp \
  lib/extension_lan_state/src/extension_lan_state.cpp \
  lib/grass_lan_state/src/grass_lan_state.cpp \
  lib/transport_contract/src/transport_contract.cpp -o .build/lan_main_pass_tests
.build/lan_main_pass_tests
