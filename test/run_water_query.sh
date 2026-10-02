#!/bin/sh
# Focused host checks for the ESP1/ESP2 water-query and six-tank target contract.
set -eu
cd "$(dirname "$0")/.."
monitor_root=$(pwd)
extension_root=${ESP2_SOURCE_ROOT:-"$monitor_root/../agent11-fishtank-extension"}
tab5_root=${TAB5_SOURCE_ROOT:-"$monitor_root/../home-platform/tab5-terminal"}
mkdir -p .build
for name in water_target water_preparation legacy_lan_state extension_protocol; do
  case "$name" in
    water_preparation) sources="$extension_root/src/water_preparation.cpp $extension_root/src/water_target.cpp" ;;
    *) sources="$extension_root/src/$name.cpp" ;;
  esac
  # The workspace paths have no spaces; source-list expansion is intentional.
  c++ -std=c++17 -Wall -Wextra -Werror -pedantic -I"$extension_root/include" \
    "$extension_root/test/test_$name.cpp" $sources -o ".build/esp2_${name}_tests"
  ".build/esp2_${name}_tests"
done
c++ -std=c++17 -Wall -Wextra -Werror \
  -Ilib/temperature_engine/include -Ilib/active_event_snapshot/include \
  -Ilib/heartbeat_contract/include -Ilib/transport_contract/include \
  test/test_daily_summary/test_daily_summary.cpp \
  lib/transport_contract/src/daily_summary.cpp lib/transport_contract/src/transport_contract.cpp \
  -o .build/daily_summary_tests
.build/daily_summary_tests
c++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include \
  -Ilib/extension_lan_state/include test/test_extension_lan_state/test_extension_lan_state.cpp \
  lib/extension_lan_state/src/extension_lan_state.cpp -o .build/extension_lan_state_tests
.build/extension_lan_state_tests
c++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include \
  -Ilib/active_event_snapshot/include -Ilib/tab5_lan_state/include \
  test/test_tab5_lan_state/test_tab5_lan_state.cpp lib/tab5_lan_state/src/tab5_lan_state.cpp \
  lib/active_event_snapshot/src/active_event_snapshot.cpp -o .build/tab5_lan_state_tests
.build/tab5_lan_state_tests
ESP2_MAIN_SOURCE="$extension_root/src/main.cpp" python3 - <<'PYTHON'
import os
from pathlib import Path
source = Path(os.environ['ESP2_MAIN_SOURCE']).read_text()
start = source.index('aquarium::extension::TargetAverage current_target() {')
end = source.index('\nString format_temperature', start)
Path('.build/esp2_current_target.inc').write_text(source[start:end])
PYTHON
c++ -std=c++17 -Wall -Wextra -Werror \
  -Ilib/temperature_engine/include -Ilib/active_event_snapshot/include \
  -Ilib/heartbeat_contract/include -Ilib/transport_contract/include \
  -I.build -Ilib/tab5_lan_state/include -Ilib/grass_lan_state/include -Ilib/extension_lan_state/include \
  -I"$extension_root/include" -I"$tab5_root/include" \
  test/test_water_query_integration.cpp lib/tab5_lan_state/src/tab5_lan_state.cpp \
  lib/extension_lan_state/src/extension_lan_state.cpp \
  lib/active_event_snapshot/src/active_event_snapshot.cpp \
  lib/transport_contract/src/daily_summary.cpp lib/transport_contract/src/transport_contract.cpp \
  "$extension_root/src/legacy_lan_state.cpp" "$extension_root/src/extension_protocol.cpp" \
  "$extension_root/src/water_target.cpp" "$extension_root/src/water_preparation.cpp" \
  "$tab5_root/src/core/agent11_lan_state.cpp" -o .build/water_query_integration_tests
.build/water_query_integration_tests
ESP1_MAIN_SOURCE="${ESP1_MAIN_SOURCE:-src/main.cpp}" python3 - <<'PYTHON'
import os
from pathlib import Path
source = Path(os.environ['ESP1_MAIN_SOURCE']).read_text()
start = source.index('    if (sampled_at >= 1700000000) {')
end = source.index('    const bool delivered = heartbeat.notify', start)
Path('.build/esp1_water_query.inc').write_text(source[start:end])
PYTHON
c++ -std=c++17 -Wall -Wextra -Werror -DARDUINO \
  -Itest/lan_runtime_stubs -I.build -Ilib/temperature_engine/include \
  -Ilib/active_event_snapshot/include -Ilib/heartbeat_contract/include \
  -Ilib/transport_contract/include -Ilib/extension_lan_state/include \
  test/test_water_query_runtime.cpp lib/extension_lan_state/src/extension_lan_state.cpp \
  lib/transport_contract/src/daily_summary.cpp lib/transport_contract/src/transport_contract.cpp \
  -o .build/water_query_runtime_tests
.build/water_query_runtime_tests
