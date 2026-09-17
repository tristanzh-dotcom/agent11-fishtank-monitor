# 单缸水温监控 MVP

这是第一缸的微雪 `ESP32-S3-DEV-KIT-N16R8-M` 固件：五个 DS18B20 水温探头、
设备直连 Bark HTTPS 告警、默认关闭的可选 MQTT，以及完全在设备本地执行的温度判断。
温度报警不依赖常驻电脑或云端转发；独立的腾讯云心跳负责在设备断电或失联时发出
离线/恢复通知。系统不控制加热棒、水泵、插排或其他市电设备。

## 本地五分钟心跳归档（独立于 Codex）

`tools/heartbeat_archive.py` 是一个只读本地采集器：每次通过现有 Agent11 HTTPS
只读 GET 获取一份 `FishTankStateV1`，并将未改写的上游 JSON 连同本地采集时间追加到
Obsidian Vault 的 JSONL 文件。Token 只从 Agent12 的受控环境文件读取，不写入 Vault、日志
或标准输出。

单次验证（先使用临时 Vault）：

```bash
tmp_vault="$(mktemp -d)"
python3 tools/heartbeat_archive.py \
  --env-file /Users/tristanzh/agent/AgentAssetVault/99_System/audit/.agent12-web.env \
  --vault "$tmp_vault"
find "$tmp_vault" -type f -maxdepth 5 -print
```

安装为 macOS 用户级五分钟任务：

```bash
cp tools/com.tz.agent11.heartbeat-archive.plist.template \
  "$HOME/Library/LaunchAgents/com.tz.agent11.heartbeat-archive.plist"
launchctl bootstrap "gui/$(id -u)" \
  "$HOME/Library/LaunchAgents/com.tz.agent11.heartbeat-archive.plist"
```

归档文件路径为：

```text
/Users/tristanzh/agent/AgentAssetVault/95_Ledgers/fishtank/heartbeat/heartbeat-YYYY-MM-DD.jsonl
```

每行保留 `source_timestamp_ms`、`display_c`、`return_c`、连接状态和活动事件；这条链路
保存的是约五分钟一次的云端心跳，不会补齐 ESP32 内部的 30 秒采样。电脑关机、休眠或断网
期间会出现本地采集缺口，恢复后任务继续追加。

## 已实现的告警策略

显示缸探头是鱼只安全的唯一判断依据，回水仓探头只做循环诊断。采样间隔为 30 秒。

| 场景 | 触发 | 级别 |
| --- | --- | --- |
| 低温 | 显示缸 `<23.5°C` 持续 15 分钟 | N2 |
| 严重低温 | 显示缸 `<22.5°C` 持续 5 分钟 | N3 |
| 高温 | 显示缸 `>27.5°C` 持续 15 分钟 | N2 |
| 严重高温 | 显示缸 `>28.5°C` 持续 5 分钟 | N3 |
| 温变过快 | 30 分钟内绝对变化 `≥1.0°C` 打开；未恢复时达到 `≥2.0°C` 升级 | N2 / N3 |
| 循环异常 | 双探头温差 `>0.8°C` 持续 10 分钟 | N2 |
| 探头失效 | 连续两次显示缸读数无效 | N2 |

高温恢复要求 `≤26.5°C` 持续 15 分钟；低温恢复要求 `≥24.5°C` 持续 15 分钟。
快速温变必须连续 30 分钟窗口变化 `<1.0°C` 满 30 分钟才恢复，并按该事件曾达到的
最高等级通知一次；循环异常须温差 `≤0.6°C` 持续 10 分钟才恢复。主缸探头无效期间，
系统只提醒探头故障，不会把旧温度判断当作恢复或继续提醒；恢复有效读数后才重新计算
既有温度事件的恢复条件。所有未恢复的 N2/N3 事件每 60 分钟最多提醒一次。夏季自然
稳定的 26–27°C 不会触发高温事件。

## 三路小缸本地 Bark 与温度汇总

除包包缸主缸和回水缸外，固件支持老四缸、小黑缸、毛毛缸三个独立的 DS18B20 监测点。
三路小缸只在设备本地运行温度状态机并通过 Bark 告警，不进入腾讯云心跳、`FishTankStateV1`
或 Web 页面；现有主缸/回水缸心跳字段和公共状态保持不变。三个小缸的 ROM 必须在现场
识别后固定填写，未配置或重复 ROM 不会按 OneWire 发现顺序代替读取。

老四缸低温 N2/N3/恢复阈值为 `<22.5°C`、`<20.5°C`、`>=23.5°C`；小黑缸和毛毛缸为
`<23.5°C`、`<22.5°C`、`>=24.5°C`。三个小缸高温阈值沿用 `>27.5°C`/`>28.5°C`，
持续、恢复和提醒时间沿用主缸策略。每日上海时间 09:00 和 18:00 的一分钟窗口内，
设备会独立生成一条包含五路读数的 Bark 汇总；汇总失败最多保留 10 分钟，不跨重启补发，
也不追补错过的时间窗口。未配置、配置错误或无效读数分别显示为“未配置”、“配置错误”
或“无有效读数”。

## 首次配置与编译

1. 使用 Python 3.10–3.13 的独立虚拟环境安装 `platformio==6.2.0`。当前已验证入口为
   `/Users/tristanzh/.platformio/penv/bin/pio`（Python 3.12）；下文 `pio` 均指该环境的命令。
   `platformio.ini` 固定 pioarduino 平台 `55.03.36`（Arduino 3.3.6），不要使用系统 Python 3.9 的旧入口。
2. 复制 `include/secrets.example.hpp` 为本机 `include/secrets.hpp`。只在该忽略文件中填写
   Wi-Fi、Bark Device Key、腾讯 SCF 函数 URL、设备共享密钥及两条 HTTPS 链路的
   CA PEM；不要把值发到聊天、日志或截图。
3. 保持 `include/config.hpp` 的 `bark_enabled = true`、
   `heartbeat_enabled = true` 和 `mqtt_enabled = false`。
   MQTT 不是 MVP 前提；仅在未来明确需要时才启用并填写其凭据与 CA。
4. 首刷可让三个新增小缸 DS18B20 ROM 地址保持全零；现有主缸和回水缸继续使用已确认的
   固定 ROM。串口会打印每只探头的 `index` 和 8 字节 ROM；确认物理角色后，将三个新增
   ROM 固定填写到 `laosi_tank`、`xiaohei_tank`、`maomao_tank`，不得依赖发现顺序。
5. 用 USB-C 数据线连接开发板，执行：

```sh
pio run -e waveshare_esp32s3_n16r8 -t upload --upload-port <已确认的设备串口>
```

开发板通过板载 CH343/CH334 将 UART 与 USB 接到同一个 USB-C。固件保留 UART0
`Serial`，不启用原生 USB CDC 重定向。先确认实际串口，再以 115200 baud 观察输出：

```sh
pio device list
pio device monitor -b 115200
```

首次上电前先阅读 [硬件安装](docs/hardware-installation.md)；接入新增三路探头前，使用
[五探头接入清单](docs/five-probe-commissioning.md) 完成物料、ROM、校准和观察记录。接入
水体后做 48–72 小时只监控浸泡验证，不改动原有加热系统。

## Bark 主路径与可选 MQTT

Bark 是 MVP 默认主路径。设备将事件以 JSON POST 发送到固定端点
`https://api.day.app/push`；Device Key 只放在经过 TLS 加密的请求体中，不出现在 URL、
串口日志或事件文本。N2 使用 `timeSensitive`，N3 使用 `critical`。

Bark 与 MQTT 各有独立 16 条 RAM 队列。Bark 成功仅移除 Bark 队首，MQTT 成功仅移除
MQTT 队首；MQTT 默认关闭，其未配置、连接失败或发布失败不会阻塞 Bark。队列不跨断电
持久化，因此复位会丢失尚未投递的温度事件。

温度告警正文固定先给出事件当时读数、告警/升级/提醒/解除判定时间，再给出本次发送发起时间；
设备直推标为“设备”，云端主缸通知标为“云端”，时间均为北京时间。设备未完成校时时显示
“时间未同步”，云端事件日历时间暂不可由现有心跳可信得出时显示“事件时间不可用”。

## 腾讯云在线状态监控

设备每 5 分钟把最新有效双探头读数通过校验证书的 HTTPS 发送到腾讯云 SCF；请求使用
HMAC-SHA256、当前 UTC 时间和一次性 nonce 鉴权。心跳失败使用 5 秒至 5 分钟的独立
指数退避，不阻塞温度采样、Bark 或可选 MQTT。

同一个上海区 `fishtank-monitor` 函数覆盖写入 COS 的唯一对象
`devices/esp1/state.json`，并由 5 分钟 Timer 检查在线状态。连续 15 分钟没有有效
心跳时 Bark 通知一次，恢复后再通知一次；不保存持续增长的云端历史日志。部署和安全
配置见 [腾讯云 SCF 说明](cloud/tencent-scf/README.md)。

2026-07-17 已完成无硬件云端验收：签名/重放/无效请求、固定 COS 状态、五分钟
Timer，以及离线一次、重复离线不提醒、恢复一次和重复恢复不提醒均已通过。

如果显式启用 MQTT，原遥测和事件 Topic/JSON 契约保持不变。`cloud/bark-forwarder/`
仅作为未部署的历史可选适配器保留，不需要为当前 MVP 创建任何云资源。

## 本地验证

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_simulation.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_simulation_tests && ./.build/temperature_simulation_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include test/test_temperature_engine/test_config.cpp -o .build/config_tests && ./.build/config_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/scoped_event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && ./.build/transport_contract_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/active_event_snapshot/include -Ilib/heartbeat_contract/include test/test_heartbeat_contract/test_heartbeat_contract.cpp lib/active_event_snapshot/src/active_event_snapshot.cpp lib/heartbeat_contract/src/heartbeat_contract.cpp -o .build/heartbeat_contract_tests && ./.build/heartbeat_contract_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/active_event_snapshot/include -Ilib/heartbeat_contract/include -Ilib/transport_contract/include test/test_daily_summary/test_daily_summary.cpp lib/transport_contract/src/daily_summary.cpp lib/transport_contract/src/transport_contract.cpp -o .build/daily_summary_tests && ./.build/daily_summary_tests
node --test cloud/bark-forwarder/index.test.mjs
(cd cloud/tencent-scf && npm test)
sh test/verify_docs.sh
pio run -e waveshare_esp32s3_n16r8
```

远程五缸温度快捷查询的刷写候选使用 `waveshare_esp32s3_n16r8_remote_five` 环境；该环境同时
保留手机云端温度摘要与 Tab5 局域网广播，默认环境继续保留现有 Tab5 功能。

## 安全边界

- 控制盒必须放在底柜外侧，传感器线做滴水弯。
- 不在潮湿底柜内放 ESP32、USB 电源或插排；不接继电器和市电。
- 主缸探头是安全报警依据，底滤探头只做循环诊断。
- 禁止在源码、串口日志、截图、URL 或事件中输出 Wi-Fi 密码、MQTT 密码、Bark key、
  腾讯设备密钥、心跳签名或函数 URL。
