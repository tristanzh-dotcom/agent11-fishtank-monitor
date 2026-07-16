# 软件开发交接（第一缸水温监控 MVP）

## 任务范围与最新批准架构

第一缸使用微雪 `ESP32-S3-DEV-KIT-N16R8-M` + 双 DS18B20 采样；主缸探头作为
安全报警依据、底滤上水泵仓探头用于循环诊断。2026-07-16 最新批准通信架构为：

`ESP32-S3 → HTTPS → Bark`

阿里云物联网平台在新账号无法开通，不再是 MVP 前提。设备不依赖常驻电脑或树莓派。
MQTT 代码保留为默认关闭的可选链路，未配置或失败不得阻塞采样、告警判断或 Bark。
Bark Device Key 尚未提供，也不得写入仓库、聊天、URL 或日志。

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
- 所有网络链路都要求 CA PEM；没有使用 `setInsecure()`。

## 本地验证命令

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_simulation.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_simulation_tests && ./.build/temperature_simulation_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include test/test_temperature_engine/test_config.cpp -o .build/config_tests && ./.build/config_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && ./.build/transport_contract_tests
node --test cloud/bark-forwarder/index.test.mjs
sh test/verify_docs.sh
/Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8
```

## 关键文件

- `platformio.ini`：S3 N16R8 构建目标。
- `include/config.hpp`：策略、采样周期、ROM 配置及默认传输模式。
- `include/secrets.hpp`：仅本机凭据；不能提交或输出真实密钥。
- `src/main.cpp`：采样和 Bark/MQTT 独立调度。
- `src/bark_notifier.*`：固定 Bark JSON POST 适配器。
- `src/aliyun_mqtt.*`：默认关闭的可选 MQTT 适配器。
- `lib/temperature_engine/`：报警状态机和策略。
- `lib/transport_contract/`：消息契约、双队列协调和退避。

## 仍需硬件或凭据的事项

1. 实物到货后验证 UART0、GPIO4、PSRAM、Wi-Fi 和双 DS18B20。
2. 记录两支 DS18B20 ROM 地址并写入 `include/config.hpp`，固定主缸/底滤角色。
3. 仅在本机 `include/secrets.hpp` 填 Wi-Fi、Bark Device Key 和 Bark CA PEM。
4. 用真机验证 N2 opened/reminder/resolved、N3、HTTP 非 2xx 重试与断网恢复队列。
5. 只有未来明确需要 MQTT 时才将 `mqtt_enabled` 改为 `true` 并配置 MQTT 凭据/CA。

## 已知边界与风险

- 两个事件队列都只在 RAM 中，ESP32 复位会丢失尚未投递事件。
- 设备断电、损坏或 Wi-Fi 完全离线时无法自行发 Bark；当前无外部在线状态监控。
- Bark HTTPS 调用有 5 秒连接/读取超时；真机阶段需观察弱网下的采样抖动。
- 未实现蜂鸣器、继电器或任何市电控制；不得在此基础上自行扩展市电控制。
- 本地构建不能证明实体 USB、PSRAM、GPIO、传感器、证书或真实 Bark 投递可用。
