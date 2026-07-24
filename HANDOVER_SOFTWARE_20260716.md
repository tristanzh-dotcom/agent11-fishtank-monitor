# 软件开发交接（第一缸水温监控 MVP）

## 任务范围与最新批准架构

第一缸使用微雪 `ESP32-S3-DEV-KIT-N16R8-M` + 双 DS18B20 采样；主缸探头作为
安全报警依据、底滤上水泵仓探头用于循环诊断。2026-07-16 最新批准通信架构为：

`ESP32-S3 → HTTPS → Bark（温度告警主路径）`

`ESP32-S3 → 腾讯云 SCF → COS / Bark（独立在线状态路径）`

阿里云物联网平台在新账号无法开通，不再是 MVP 前提。设备不依赖常驻电脑或树莓派。
MQTT 代码保留为默认关闭的可选链路，未配置或失败不得阻塞采样、告警判断或 Bark。
Bark Device Key 已由用户在腾讯云控制台私下输入，仅以掩码显示；不得写入仓库、
聊天、URL 或日志。

## 已批准报警策略

| 事件 | 条件 | 告警等级 |
|---|---|---|
| 高温 | >27.5°C 持续 15 分钟 | N2 |
| 严重高温 | >28.5°C 持续 5 分钟 | N3 |
| 低温 | <23.5°C 持续 15 分钟 | N2 |
| 严重低温 | <22.5°C 持续 5 分钟 | N3 |
| 高温恢复 | ≤26.5°C 持续 15 分钟 | resolved |
| 低温恢复 | ≥24.5°C 持续 15 分钟 | resolved |
| 温变 | 30 分钟内 ≥1°C / ≥2°C | N2 / N3 |
| 双探头差异 | >0.8°C 持续 10 分钟 | N2 |
| 主缸探头故障 | 连续 2 次无效样本 | N2 |
| N2/N3 提醒 | 事件持续未恢复时每 60 分钟最多一次 | reminder |

夏季主缸常在 26–27°C，冬季约 25°C；上述阈值是已批准的 MVP 默认值。

### 2026-07-19 已批准的事件闭环语义

为修复一次性锁存事件并避免将缺测误报为温度恢复，TZ 已批准以下规则：

- 快速温变是一个以最高等级表示的事件。初次在 30 分钟窗口达到 `>=1.0°C`
  打开 N2；未恢复前若达到 `>=2.0°C`，追加一次 N3 升级通知。只有连续完整
  30 分钟窗口的绝对温差持续小于 `1.0°C` 满 30 分钟后，才发送一次按该事件
  最高等级标记的 `resolved`。
- 双探头温差在 `>0.8°C` 连续 10 分钟后打开 N2；必须在 `<=0.6°C` 连续
  10 分钟后才发送 `resolved`。任一恢复观察样本回到 `>0.6°C` 会重新计时。
- 主缸探头连续两次无效样本后打开 `sensor_fault` N2。无效期间不以旧温度发出
  高低温、温变或温差 reminder，也不把这些事件标为恢复；所有未完成的触发、恢复
  及快速温变基线计时均清除。`sensor_fault` 本身每 60 分钟最多 reminder 一次。
  主缸读数恢复有效时立即发送 `sensor_fault resolved`；此前已打开的温度事件仅在
  恢复后的连续有效样本满足其既定恢复规则时才可发送 `resolved`。

## 当前实现状态

- `TemperatureEngine` 状态机覆盖高/低温、严重高/低温、恢复滞回、快速温变、
  探头差异、探头故障和 reminder；此次传输适配不修改这些规则。
- PlatformIO 默认目标为 `waveshare_esp32s3_n16r8`：兼容板定义
  `esp32-s3-devkitc-1`、16 MB Flash、8 MB OPI PSRAM、UART0 115200、OneWire GPIO4。
- 双 DS18B20 启动时打印 ROM 地址，供后续固定主缸/底滤角色。
- Wi-Fi 使用 1 秒至 60 秒指数退避。
- Bark 是 MVP 默认主路径，使用固定 `https://api.day.app/push`、CA 校验和 JSON POST；
  Device Key 只进入 HTTPS 请求体。
- Bark 与可选 MQTT 使用各自独立的 16 条 RAM 事件队列，各自成功后才确认自己的队首；
  MQTT 失败不会阻塞 Bark。
- MQTT 默认关闭；启用后保留原遥测与事件 Topic 及消息契约。
- `cloud/bark-forwarder/` 保留为未部署的历史可选适配器，不属于当前 MVP 必需路径。
- 腾讯云心跳默认启用：每 5 分钟发送最新有效双探头读数；失败使用 5 秒至 5 分钟
  独立指数退避，不阻塞 Bark/MQTT。
- 心跳使用 NTP、32 位十六进制随机 nonce、SHA-256 与 HMAC-SHA256；函数 URL、
  设备密钥和 CA 只存在于本机忽略文件。
- 上海区 SCF 使用固定 COS 对象 `devices/tank01/state.json`，15 分钟无心跳时
  只通知一次离线，恢复后只通知一次。
- 所有网络链路都要求 CA PEM；没有使用 `setInsecure()`。
- 上海区函数 URL 已启用；SCF 运行时读取运行环境注入的临时角色凭据，执行超时为
  10 秒。
- 2026-07-17 软件模拟真实验收：有效签名 HTTP 200、重放 HTTP 409、无效签名
  HTTP 401；COS 固定对象保存 26.4/26.6°C 和在线状态，无效请求没有污染状态。
- 云端 `BARK_KEY` 已私下保存；五分钟 Timer
  `fishtank-monitor-offline-check` 已启用。
- 受控状态已验证首次离线、重复离线 no-op、有效心跳恢复、首次恢复和重复恢复
  no-op。离线/恢复状态均只在 Bark 请求成功后写回 COS，因此云端 Bark 闭环已完成。

## 本地验证命令

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_simulation.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_simulation_tests && ./.build/temperature_simulation_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include test/test_temperature_engine/test_config.cpp -o .build/config_tests && ./.build/config_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && ./.build/transport_contract_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/heartbeat_contract/include test/test_heartbeat_contract/test_heartbeat_contract.cpp lib/heartbeat_contract/src/heartbeat_contract.cpp -o .build/heartbeat_contract_tests && ./.build/heartbeat_contract_tests
node --test cloud/bark-forwarder/index.test.mjs
(cd cloud/tencent-scf && npm test)
sh test/verify_docs.sh
/Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8
```

## 关键文件

- `platformio.ini`：S3 N16R8 构建目标。
- `include/config.hpp`：策略、采样周期、ROM 配置及默认传输模式。
- `include/secrets.hpp`：仅本机凭据；不能提交或输出真实密钥。
- `src/main.cpp`：采样和 Bark/MQTT/心跳独立调度。
- `src/bark_notifier.*`：固定 Bark JSON POST 适配器。
- `src/heartbeat_notifier.*`：NTP、HMAC-SHA256 和腾讯 SCF HTTPS 适配器。
- `src/aliyun_mqtt.*`：默认关闭的可选 MQTT 适配器。
- `lib/temperature_engine/`：报警状态机和策略。
- `lib/transport_contract/`：消息契约、双队列协调和退避。
- `lib/heartbeat_contract/`：心跳 JSON/canonical 合同和独立调度。
- `cloud/tencent-scf/`：单函数心跳接收、固定 COS 状态及离线/恢复检查。

## 仍需硬件或真机凭据的事项

1. 实物到货后验证 UART0、GPIO4、PSRAM、Wi-Fi 和双 DS18B20。
2. 记录两支 DS18B20 ROM 地址并写入 `include/config.hpp`，固定主缸/底滤角色。
3. 仅在本机 `include/secrets.hpp` 填 Wi-Fi、Bark Device Key、两条 HTTPS 链路的
   CA PEM、SCF 函数 URL 和与云端一致的设备共享密钥。
4. 烧录前轮换一次最终设备共享密钥，并只在腾讯云环境变量和本机忽略文件中同步。
5. 用真机验证 NTP、心跳 HMAC、5 分钟周期、HTTP 非 2xx 退避和重新联网恢复。
6. 用真机验证 N2 opened/reminder/resolved、N3、Bark 非 2xx 重试与断网恢复队列。
7. 进行双探头校准、桌面运行和鱼缸 48–72 小时观察。
8. 只有未来明确需要 MQTT 时才将 `mqtt_enabled` 改为 `true` 并配置 MQTT 凭据/CA。

## 已知边界与风险

- 两个事件队列都只在 RAM 中，ESP32 复位会丢失尚未投递事件。
- 设备断电、损坏或 Wi-Fi 完全离线时由腾讯云根据最后心跳报警；最终真机闭环仍须
  等硬件到货后验证。
- Bark HTTPS 调用有 5 秒连接/读取超时；真机阶段需观察弱网下的采样抖动。
- 未实现蜂鸣器、继电器或任何市电控制；不得在此基础上自行扩展市电控制。
- 本地构建不能证明实体 USB、PSRAM、GPIO、传感器、证书或真实 Bark 投递可用。
