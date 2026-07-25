# 软件与云链路收工交接（第一缸温度监控 MVP，2026-07-24）

## 当前阶段

第一缸已完成真实 ESP32-S3 双 DS18B20 桌面验收，软件工作流从“硬件适配”切换为
“真机网络与云链路验证”。温度状态机、Bark 直连、腾讯 SCF 在线心跳和下游状态合同
均有本地实现；真实 Wi-Fi、NTP、Bark 和 SCF 的设备端闭环尚未在今天断电后的设备上
继续验证。

权威现场硬件事实见 `HANDOVER_HARDWARE_LIVE_STATUS_20260724.md`；它覆盖旧交接中
“等待 ROM/实物”的事项。

## 已完成的软件状态

- S3 目标为 `waveshare_esp32s3_n16r8`；OneWire 使用 GPIO4。
- 真实 ROM 已写入 `include/config.hpp`：
  - 主缸安全探头：`28 20 D5 6B 11 00 00 C1`。
  - 底滤诊断探头：`28 17 AE 6B 11 00 00 B4`。
- 串口采样输出采用 `main_tank` / `sump_tank` 名称；内部
  `TemperatureSample.display_c` / `return_c` 和既有安全角色不改变。
- `TemperatureEngine` 已实现：高低温、N2/N3、恢复、提醒、快速温变 N2→N3 升级、
  30 分钟稳定恢复、温差 0.8/0.6°C 滞回恢复、主探头故障提醒和缺测期间的连续性重置。
- 直接 HTTPS Bark 是温度告警主路径；MQTT 默认关闭，不能阻塞采样或 Bark。
- 腾讯 SCF/COS 是独立在线状态路径；每 5 分钟发送带完整活动事件快照的心跳。任一
  探头缺测时读数为 `null`，心跳仍发送，15 分钟无有效心跳由云端通知一次离线，恢复后
  通知一次。
- 本地代码已新增独立 Bearer 保护的
  `GET /api/v1/devices/tank01/state`，将私有 v2 COS 状态投影为下游 `FishTankStateV1`；
  `STATE_READ_TOKEN` 与设备 HMAC 和 Bark key 必须彼此不同。
- `docs/contracts/fishtank-state-v1.schema.json` 及 fixtures 已提供给 Agent12 等下游
  消费者；真实缺失读数必须表示为 `null`，不得伪造温度。

## 已有验证证据

- 今日真实设备：实际烧录成功；两支 DS18B20 同总线稳定读取 2 小时，无重启、无
  `invalid`、无离谱数值；同杯水双探头差约 0.06°C。
- 本地软件：温度状态机、仿真、配置、传输契约、心跳契约、Bark 转发器、腾讯 SCF
  Node 测试、文档检查与 ESP32-S3 PlatformIO 构建此前均已通过。2026-07-25 的本地
  改动另加入了活动事件快照、2 KiB 心跳上限和只读状态 API；其部署和真机闭环尚未发生。
- 这些证据不等同于真机 Wi-Fi/NTP/TLS/Bark/SCF 的端到端成功。

## 当前物理状态

- 串口监视已退出，ESP32 USB-C 已拔掉，电路断电。
- 尚未装入防水盒、未下缸、未接最终 RVV/WAGO 线束，未做 48–72 小时只监控观察。
- 不授权任何 220V、继电器、SSR、加热棒或水泵控制开发。

## 下一次工作顺序

1. 使用已确认的主串口重新接入设备，确认固定 ROM 和 `main_tank/sump_tank` 串口输出。
2. 在本机忽略的 `include/secrets.hpp` 配置已授权的 Wi-Fi、CA、Bark 和腾讯心跳凭据；
   不在源码、聊天、日志、URL 或提交中输出它们。
3. 经单独腾讯云确认后，上传新版 SCF 包并在控制台私下配置独立
   `STATE_READ_TOKEN`；不可把它放进 ESP32 或复用 HMAC/Bark 密钥。
4. 真机验证 Wi-Fi 连接、NTP 合理时间、腾讯 SCF HMAC、5 分钟周期、HTTP 非 2xx
   退避，以及断网后恢复。
5. 真机验证 Bark：N2 opened/reminder/resolved、N3 升级、Bark 失败重试、断网恢复后的
   RAM 队列行为。
6. 用独立的 `STATE_READ_TOKEN` 验证只读 GET 投影；然后再进行防水盒安装和鱼缸
   48–72 小时只监控观察；不改变原有加热或水泵系统。

## 已知边界

- Bark/MQTT 事件队列只驻留 RAM；设备复位会丢失尚未投递事件。
- 心跳在任一探头缺测时仍会发送 `null` 读数和 `sensor_fault` 快照；若网络心跳长期
  缺失，云端仍会按在线状态规则判离线。
- HTTPS 调用有 5 秒连接/读取超时；真机弱网时需观察采样抖动。
- 当前校准只支持 MVP 继续验证，未引入校准偏移，以免改变已批准报警边界。

## 下次开场提示

“请读取 `HANDOVER_HARDWARE_LIVE_STATUS_20260724.md`、
`HANDOVER_SOFTWARE_CLOUD_STATUS_20260724.md` 和
`HANDOVER_SOFTWARE_20260716.md`。第一缸真实 ESP32-S3 双 DS18B20 已完成桌面验收，
ROM 已固定，当前应继续真机 Wi-Fi/NTP/腾讯 SCF/Bark 网络链路验证。不得输出密钥，
不得引入 220V 控制。”
