# 单缸温度监控 MVP 设计

## 2026-07-16 最新批准的软件架构

本节由 TZ 在 2026-07-16 的软件任务委派中明确批准，并覆盖本文后续历史描述中
“阿里云 IoT Platform 是 MVP 前提”的旧决策。新账号无法开通阿里云物联网平台，
因此第一缸 MVP 使用：

`ESP32-S3-DEV-KIT-N16R8-M → HTTPS → Bark`

设备温度判断和温度告警不依赖常驻电脑、树莓派或云端转发。另增加独立的
`ESP32-S3 → 腾讯云 SCF → COS / Bark` 心跳路径，用于设备离线和恢复通知。
阿里云 MQTT 保留为默认关闭的可选链路；心跳或 MQTT 的未配置、连接失败或发布失败
都不得阻塞本地采样、告警判断或 Bark 投递。

## 已批准边界

- 第一阶段只监控一只 80 × 50 × 55 cm、带底滤的鱼缸；不控制加热棒、水泵或其他市电设备。
- 主控为微雪 `ESP32-S3-DEV-KIT-N16R8-M`（16 MB Flash + 8 MB PSRAM）。
- 使用两支三线制 DS18B20；主缸探头是水温告警的唯一权威测点，底滤探头只用于循环诊断。
- Bark 是 MVP 的默认主投递链路；MQTT 默认关闭且独立运行。
- 腾讯云心跳每 5 分钟独立发送，15 分钟无有效心跳时通知一次离线，恢复后通知一次。
- 不使用真实 Bark Device Key，不写入外部云资源，不在源码、日志、URL 或事件文本中暴露密钥。
- TLS 必须校验证书，不允许 `setInsecure()`。

## 2026-07-16 最终硬件契约

- 主控为两块微雪 `ESP32-S3-DEV-KIT-N16R8-M`；`N16R8` 是 16 MB Flash +
  8 MB PSRAM，`-M` 是已焊排针版本。PlatformIO 不再保留 C3 当前目标。
- 探头为三支三线、非寄生供电的 DFRobot `KIT0021` DS18B20；安装两支，第三支备件。
- 两支已安装探头共用 GPIO4 OneWire，总线使用一只 4.7 kΩ 1/4 W 电阻从 DATA
  上拉至 3.3 V。
- ROM 配置入口命名为 `main_tank_sensor` 与 `sump_return_sensor`。前者映射到既有
  `display_c` 安全报警契约，后者映射到既有 `return_c` 循环诊断契约；不改变遥测 JSON。
- 原型使用 MB-102、杜邦线、WAGO `221-413` 和 5 米 `RVV 三芯 0.3平方` 延长线，
  不再使用 KF301，也不焊接。
- 防水盒面板线为 0.3 米、标题含 `2.0USB-C`、`公对母`、`数据线` 的 USB-C
  延长线；用户已有 USB-A 5 V 2 A 以上电源和 USB-A 转 USB-C 数据线。
- 不接继电器、SSR、加热棒、水泵或任何 220 V 控制。

## 温度策略

25.0°C 是理想目标，但夏季环境下 26–27°C 是可接受的观测状态，不应产生重复推送。

| 规则 | 默认值 | 持续时间 | 结果 |
| --- | --- | --- | --- |
| 低温注意 | `< 23.5°C` | 15 分钟 | N2 / Bark timeSensitive |
| 高温注意 | `> 27.5°C` | 15 分钟 | N2 / Bark timeSensitive |
| 低温紧急 | `< 22.5°C` | 5 分钟 | N3 / Bark critical |
| 高温紧急 | `> 28.5°C` | 5 分钟 | N3 / Bark critical |
| 急变注意 | `>= 1.0°C / 30 分钟` | 即时 | N2 / Bark timeSensitive |
| 急变紧急 | `>= 2.0°C / 30 分钟` | 即时 | N3 / Bark critical |
| 双探头温差 | `> 0.8°C` | 10 分钟 | N2 / Bark timeSensitive |
| 探头失效 | 连续 2 个采样无效 | 约 1 分钟 | N2 / Bark timeSensitive |

高温事件在温度 `<= 26.5°C` 持续 15 分钟后恢复；低温事件在温度
`>= 24.5°C` 持续 15 分钟后恢复。每种事件首次触发、持续未恢复时每 60 分钟
最多提醒一次、恢复时通知一次。

## 架构与数据流

`TemperatureEngine` 是与硬件无关的确定性 C++ 状态机。ESP32-S3 每 30 秒读取两支
DS18B20，将产生的语义事件同时写入两个彼此独立的 16 条 RAM 队列：

1. Bark 队列始终作为主路径；只有 Bark 返回 HTTP 2xx 后才确认并移除队首事件。
2. MQTT 队列只在 `mqtt_enabled=true` 时接收事件；它的失败、退避和确认不影响 Bark 队列。
3. MQTT 遥测只在显式启用时发布，失败不影响采样与 Bark。

Bark 使用固定 `https://api.day.app/push` 端点和 JSON POST。Device Key 只放在 HTTPS
请求体的 `device_key` 字段中，不拼入 URL。设备仅保留 RAM 队列，复位仍会丢失尚未投递事件；
跨冷启动持久化不在 MVP 范围内。

## 方案取舍

- 已选：Bark/MQTT 双独立队列。它使可选 MQTT 不阻塞主路径，同时保留两条链路各自的失败重试。
- 未选：单一 Bark 队列加 MQTT 即时 best-effort，因为 MQTT 失败事件无法重试。
- 未选：通用多目标 fan-out 框架，因为第一缸 MVP 只有两个固定目标，抽象成本超过收益。

## 配置与安全

- Wi-Fi、Bark Device Key、Bark CA PEM 与可选 MQTT 凭据只存在于本机
  `include/secrets.hpp`；代码库只提供无效占位模板 `include/secrets.example.hpp`。
- 默认运行配置为 `bark_enabled=true`、`mqtt_enabled=false`。
- Bark 文本不包含密钥、Wi-Fi 密码、本地路径或原始错误载荷。
- MQTT 和 HTTPS 使用证书校验；固件不允许 `setInsecure()`。
- 不实现继电器、蜂鸣器或任何市电控制。

## 验收

- 现有状态机和仿真测试原样通过，证明报警策略没有漂移。
- 新的主机测试先失败再通过，覆盖默认传输模式、双队列独立确认和 Bark JSON POST 契约。
- 文档检查证明当前用户指引以 S3 + 直连 Bark 为主，且不再把阿里云写成 MVP 前提。
- PlatformIO 的 `waveshare_esp32s3_n16r8` 完整构建通过。
- 不执行真实网络投递；实体板、探头 ROM、Wi-Fi、Bark Key/CA 与真机投递留待硬件阶段验证。
- 配置测试证明默认 ROM 未绑定，并能分别写入 `main_tank_sensor` 与
  `sump_return_sensor`；S3 构建证明读取器消费这两个入口。
