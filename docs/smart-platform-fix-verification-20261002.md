# 智能平台最小修复：ESP1 软件验证（2026-10-02）

范围依据本次 TZ 批准的审计修复：N3 未恢复提醒和 ESP2/3 连接通知重试；基线 `8ba884465f5f87eb50b6a51b163b568e167f9385`，开始工作树 clean。仅软件实现和候选构建，未提交、未部署、未刷写、未打开串口、未真实发送 Bark。

- 已打开的高/低 N3 离开 critical 触发区间但未满足既有恢复条件，仍每小时提醒；恢复完成后停止。阈值、持续时间、无效探头分支、最高等级保持不变。
- ESP2/3 各有一个连接通知 pending：首次加两次重试，失败完成后间隔 30 秒；成功清除，新连接转换覆盖旧待发状态。判定时间保留，实际尝试刷新发送时间。接收事实与投递事实分开；没有扩展温度事件队列。
- `test_water_query_integration.cpp` 的时钟接口对齐 ESP2 单调时间入口；只是直接消费者主机接口适配。

验证等级 Level 2。真实主循环片段由现有 runner 提取，使用真实 receiver、tracker、消息构造和 HMAC，只替换网络/HTTP 等外部接口。新温度和失败通知重现先失败后通过。覆盖高/低滞回未恢复、恢复清除、两来源失败限次/间隔、恢复覆盖未发离线通知、同步 Bark 期间到达 ESP3 新包。

已执行（均 exit 0）：

```sh
bash test/run_lan_receive.sh
clang++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && .build/temperature_engine_tests
clang++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_simulation.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_simulation_tests && .build/temperature_simulation_tests
clang++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/scoped_event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && .build/transport_contract_tests
```

LAN runner 输出 8 项 PASS，按主执行上下文最终日志要求再次执行后完整日志为 `/Users/tristanzh/agent/agent11-fishtank-monitor/.build/lan-receive-final.log`，exit 0；温度、温度模拟、transport contract 均输出 passed。跨仓直接消费者验证如下（生成片段只提取 ESP2 current_target，未改其消费者合同）：

```sh
ESP2_MAIN_SOURCE=/Users/tristanzh/agent/agent11-fishtank-extension/src/main.cpp python3 - <<'PYTHON'
import os
from pathlib import Path
source=Path(os.environ['ESP2_MAIN_SOURCE']).read_text()
a=source.index('aquarium::extension::TargetAverage current_target() {')
b=source.index('\nString format_temperature',a)
Path('.build/esp2_current_target.inc').write_text(source[a:b])
PYTHON
clang++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/active_event_snapshot/include -Ilib/heartbeat_contract/include -Ilib/transport_contract/include -I.build -Ilib/tab5_lan_state/include -Ilib/grass_lan_state/include -Ilib/extension_lan_state/include -I../agent11-fishtank-extension/include -I../home-platform/tab5-terminal/include test/test_water_query_integration.cpp lib/tab5_lan_state/src/tab5_lan_state.cpp lib/extension_lan_state/src/extension_lan_state.cpp lib/active_event_snapshot/src/active_event_snapshot.cpp lib/transport_contract/src/daily_summary.cpp lib/transport_contract/src/transport_contract.cpp ../agent11-fishtank-extension/src/legacy_lan_state.cpp ../agent11-fishtank-extension/src/extension_protocol.cpp ../agent11-fishtank-extension/src/water_target.cpp ../agent11-fishtank-extension/src/water_preparation.cpp ../home-platform/tab5-terminal/src/core/agent11_lan_state.cpp -o .build/water_query_integration_tests && .build/water_query_integration_tests
```

结果 `water query integration and Tab5 compatibility tests passed`，exit 0。

目标构建：`/Users/tristanzh/.platformio/penv/bin/pio run -e waveshare_esp32s3_n16r8_remote_five`，最终 SUCCESS、exit 0，20.519 秒。日志 `.build/safety-firmware-build.log`。本轮共享包自动补装后，按主执行上下文要求只清理此 owning 环境，再重建以排除缓存混编：`/Users/tristanzh/.platformio/penv/bin/pio run -e waveshare_esp32s3_n16r8_remote_five -t clean` exit 0（11.391 秒）；`/Users/tristanzh/.platformio/penv/bin/pio run -e waveshare_esp32s3_n16r8_remote_five -v` exit 1（28.311 秒），失败仅在 SCons print_cmd_line 的 `_Null` 与 str 相加，日志 `.build/safety-firmware-verbose-failed.log`。没有改工具或 manifest，改回上述标准构建命令后最终 SUCCESS。verbose 实际 include/link 路径和最终 `firmware.map` 都仅指向 `/Users/tristanzh/.platformio/packages/framework-arduinoespressif32-libs.agent11-3.3.6-backup`，其 package manifest 为 `5.5.0+sha.f56bea3d1f`，匹配 owning platform `55.03.36`；framework 实际为 `3.3.6`、tool-esptoolpy `5.1.0`、toolchain `14.2.0+20251107`。库根另存的 `5.4.0` 不在该最终链接的库路径中。清理日志 `.build/safety-firmware-clean.log`。依赖 OneWire 原有两项 extra-tokens-at-undef 警告保留。候选 `/Users/tristanzh/agent/agent11-fishtank-monitor/.pio/build/waveshare_esp32s3_n16r8_remote_five/firmware.bin`，SHA-256 `76eef7d843575bc35235347e3a783f1d99d05fb1253fe990f3890b2c15bb6236`。

检查覆盖受影响引擎、连接事实/通知编排、既有 transport 和养水目标直接消费者；没有 Level 4 触发，不运行全部云/工作区套件。默认 monitor 环境与 remote_five 在当前 platformio.ini 中共享相同源和 build_flags，未另构建默认/诊断环境。仍缺运行设备、真实 Bark 送达、断线恢复、跨 49.7 天与长时间稳定性/物理验收；主机和构建通过不能代替这些证据。
