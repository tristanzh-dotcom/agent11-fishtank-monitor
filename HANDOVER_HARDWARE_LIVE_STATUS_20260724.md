# 硬件实测状态交接（第一缸温度监控 MVP，2026-07-24）

## 交接目的

本文件交接给后续软件/云服务端工作流使用。硬件桌面验收已经完成到“双 DS18B20
稳定读取、ROM 固定、串口输出命名明确”的状态；可以基于真实硬件读数继续做
云端心跳、Bark 告警、状态上报和后续端到端验证。

本文件不授权任何 220V 控制开发。当前 MVP 仍然只做低压温度监控，不接继电器、
SSR、加热棒控制、回水泵控制或市电测量。

## 今日硬件实测结论

- ESP32-S3：微雪 `ESP32-S3-DEV-KIT-N16R8-M` 已实际接入电脑并烧录成功。
- macOS 串口识别正常：
  - `/dev/cu.usbmodem5B910349491`：`USB Single Serial`，用于 PlatformIO 上传和串口监视。
  - `/dev/cu.usbmodem11101`：`USB JTAG/serial debug unit`，本轮未作为主串口使用。
- 出厂 Demo 曾正常启动，随后已被本项目温控固件覆盖。
- PSRAM/USB/串口/固件启动链路可用。
- OneWire 固定使用 `GPIO4`。
- 两支 DFRobot `KIT0021` DS18B20 已并联在同一 OneWire 总线：
  - VCC 接 `3V3`
  - GND 接 `GND`
  - DATA 接 `GPIO4`
  - DATA 到 `3V3` 只使用一只 `4.7kΩ` 上拉电阻
- 两支探头桌面连续运行 2 小时稳定，无重启、无 `invalid`、无离谱温度。

## 实测探头角色与 ROM

固件配置已经按实物 ROM 固定主缸/底滤角色，避免后续因发现顺序变化导致角色对调。

| 角色 | 串口名称 | ROM | 实测用途 |
|---|---|---|---|
| 主缸探头 | `main_tank` | `28 20 D5 6B 11 00 00 C1` | 安全报警依据 |
| 底滤探头 | `sump_tank` | `28 17 AE 6B 11 00 00 B4` | 循环/温差诊断 |

识别输出示例：

```text
DS18B20 devices found: 2
DS18B20 index 0 ROM: 28 20 D5 6B 11 00 00 C1
DS18B20 index 1 ROM: 28 17 AE 6B 11 00 00 B4
```

固定 ROM 后的采样输出示例：

```text
sample at_ms=60042 main_tank=24.25C sump_tank=24.25C
```

## 校准与稳定性记录

15 分钟同杯水校准记录：

```text
独立温度计: 25.0C
main_tank: 25.31C
sump_tank: 25.37C
```

偏差判断：

- `main_tank` 相对独立温度计约 `+0.31C`
- `sump_tank` 相对独立温度计约 `+0.37C`
- 双探头之间约 `0.06C`

结论：MVP 可继续。当前不需要阻塞云端开发；如后续需要更精确显示，可单独增加校准
偏移配置，但不要影响已经批准的报警边界和事件语义。

## 已写入仓库的本轮硬件相关改动

本轮为了支持硬件验收，已修改以下文件：

- `include/config.hpp`
  - 写入两支真实 DS18B20 ROM。
  - 固定 `main_tank_sensor` 和 `sump_return_sensor`。
- `src/main.cpp`
  - 增加串口采样输出。
  - 输出名称从旧语义 `display/return` 改为更直观的 `main_tank/sump_tank`。
  - 只改变串口标签，不改变底层 `TemperatureSample.display_c` / `return_c`
    字段，也不改变报警逻辑。

当前输出格式：

```text
sample at_ms=<毫秒> main_tank=<温度或 invalid> sump_tank=<温度或 invalid>
```

## 已执行并通过的命令

固件编译通过：

```sh
/Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8
```

固件上传成功：

```sh
/Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8 -t upload --upload-port /dev/cu.usbmodem5B910349491
```

串口监视命令：

```sh
/Users/tristanzh/Library/Python/3.9/bin/pio device monitor -p /dev/cu.usbmodem5B910349491 -b 115200
```

注意：PlatformIO 当前环境中 `pio run` 不接受 `-p` 参数；上传端口应使用
`--upload-port`。

## 当前物理状态

今日收工时已完成：

- 串口监视器退出。
- ESP32 USB-C 已拔掉。
- 电路断电。
- 两支探头已完成同杯水校准和 2 小时稳定运行。

尚未完成：

- 未装入防水盒。
- 未钻孔。
- 未接 RVV 延长线。
- 未使用 WAGO 做最终盒内拼接。
- 未下缸。
- 未做 48-72 小时鱼缸只监控观察。

## 给软件/云服务端工作流的继续任务

可以基于当前硬件状态继续以下工作：

1. 以真实硬件串口输出为依据，确认固件字段命名、云端状态字段和前端/下游消费名称是否需要同步为 `main_tank` / `sump_tank`。
2. 继续真机网络链路验证：
   - Wi-Fi 连接
   - NTP 时间同步
   - 腾讯 SCF 心跳 HMAC
   - 5 分钟心跳周期
   - 非 2xx 退避
   - 断网后恢复
3. 继续 Bark 真实告警链路验证：
   - N2 opened
   - N2 reminder
   - resolved
   - N3
   - Bark 非 2xx 重试
   - 断网恢复后的队列行为
4. 确认云端状态输出不要泄露：
   - Wi-Fi 密码
   - Bark Device Key
   - 腾讯 SCF 函数 URL
   - 设备共享密钥
   - HMAC 签名
   - CA PEM
5. 若需要为 Agent12 或其他消费者提供状态合同，继续沿用“真实缺失为 `null`，
   不伪造读数”的原则。

## 软件工作流开场提示

“请读取 `HANDOVER_HARDWARE_LIVE_STATUS_20260724.md` 和
`HANDOVER_SOFTWARE_20260716.md`，基于第一缸真实硬件已完成双 DS18B20 桌面验收、
ROM 已固定、串口输出为 `main_tank/sump_tank` 的最新状态，继续云服务端和真机网络
链路开发。不得输出密钥，不得引入任何 220V 控制。”

