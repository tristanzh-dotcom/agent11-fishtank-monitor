# 心跳全量状态快照与只读接口设计

**日期：** 2026-07-25  
**状态：** TZ 已确认采用方案 A  
**上游约束：** `docs/superpowers/specs/2026-07-20-agent11-agent12-read-api-contract-design.md`

## 目标

让 ESP32 的每一次已签名心跳携带最新双探头读数和完整活跃事件快照；腾讯 SCF 只负责
验证、覆盖保存和对外投影。SCF 通过同一 Function URL 提供只读
`GET /api/v1/devices/tank01/state`，向 Agent12 返回 `FishTankStateV1`。

本设计不改变温度阈值、事件状态机、Bark 主告警路径、MQTT 默认关闭状态、探头角色或任何
市电控制边界。

## 已确认架构

```text
TemperatureEngine --事件流--> ActiveEventSnapshot（ESP32）
DS18B20 读数 ----------------> HeartbeatPayload（每 5 分钟）
                                      |
                                      | HMAC-SHA256 POST，最大 2 KiB
                                      v
                               Tencent SCF writer
                                      |
                                      v
                    私有 COS devices/tank01/state.json
                                      |
                                      | Bearer 只读 GET
                                      v
                         FishTankStateV1（Agent12）
```

云端绝不根据温度重新推导告警；Agent12 绝不根据温度重新推导告警或在线状态。

## ESP32 状态快照

新增独立的 `ActiveEventSnapshot`，它消费 `TemperatureEngine::ingest()` 返回的既有
`TemperatureEvent`，不复制任何报警判断。

- `opened`、`escalated`、`reminder`：按 `EventType` 覆盖或新增一条活跃事件；保存该次
  事件的状态、等级、时间和主缸温度。
- `resolved`：按 `EventType` 移除该事件。
- `sensor_fault` 的快照温度强制为 `null`，即使旧内部事件结构临时使用 `0.0` 作为占位。
- 最多保存七种批准的事件类型，每种类型最多一条。

每一次心跳发送：

```json
{
  "device_id": "tank01",
  "sent_at_ms": 0,
  "nonce": "...",
  "main_c": 26.4,
  "sump_c": 26.6,
  "uptime_ms": 0,
  "active_events": []
}
```

`main_c` 和 `sump_c` 均允许为 JSON `null`。当任一探头无可信读数时，ESP32 仍按原有
5 分钟节奏发送心跳和完整事件快照；这使云端能表达探头故障，而不是把设备误判为离线。
HMAC 的 canonical string 仍覆盖原始请求体 SHA-256，因此新字段受到相同签名保护。

设备到 SCF 的请求体上限从 1 KiB 提升到 2 KiB，以容纳最多七条活跃事件；SCF 仍拒绝更大
请求、未知字段语义、无效枚举、重复事件类型和伪造的 `sensor_fault` 温度。

## SCF 私有状态

新心跳将覆盖单一 COS 对象，并写入内部 `schemaVersion: 2` 状态：

- `mainC` / `sumpC`：有限数字或 `null`；
- `activeEvents`：经验证的完整活跃事件快照；
- 原有 `lastSeenAtMs`、`connectivityStatus`、`recoveryPending`、`recentNonces` 和
  `uptimeMs` 继续保留。

离线检查器继续只改变在线状态与恢复标记，保留温度和活跃事件。旧的
`schemaVersion: 1` COS 对象没有可信活跃事件快照；在第一份 v2 真实心跳覆盖前，读接口
返回 `503 state_unavailable`，绝不伪造空事件数组。

## 只读状态 API

同一个 Function URL 按 HTTP method 与路径分流：

- `POST`：保持现有 HMAC 心跳写入；
- `GET /api/v1/devices/tank01/state`：仅允许 Bearer Token 读取；
- 所有其他 HTTP method 或路径：按已批准的稳定错误合同返回 `400` 或 `405`，不读写 COS。

环境变量 `STATE_READ_TOKEN` 是独立于 ESP32 HMAC 密钥和腾讯云角色凭据的只读 Token；
长度限制为 32–256 字符，只能用常量时间比较。成功响应严格投影为
`FishTankStateV1`：

- `mainC -> display_c`，`sumpC -> return_c`；
- `lastSeenAtMs -> timestamp_ms`；
- `activeEvents -> events`；
- 事件等级 `n2/n3` 已在设备端保持小写；
- `main_tank/sump_tank` 仅保留为串口标签，不改变 V1 公共字段名。

响应、异常和日志不得包含 Token、HMAC、Wi-Fi、CA PEM、COS key 或腾讯云凭据。

## 测试与验收

本地开发先完成以下可执行验证：

1. C++：事件快照的打开、升级、提醒、恢复移除与 `sensor_fault: null`。
2. C++：心跳 JSON 的 `null` 温度、全量事件、2 KiB 大小上限和签名 canonical string。
3. Node：SCF 拒绝错误状态快照并持久化有效状态。
4. Node：只读 GET 的 `200/401/404/405/503`、Token 常量时间比较、无 COS 写入和
   `FishTankStateV1` 投影。
5. PlatformIO：ESP32-S3 编译成功。

需要重新连接开发板后才执行：真实 Wi-Fi/NTP、首份 v2 心跳、SCF/COS、Bark、断网恢复和
Agent12 真实读取联调。部署腾讯云、输入/轮换 Token 或设备密钥也留在该硬件阶段前的
单独确认点。

## 明确非目标

- 不新增历史日志、WebSocket、MQTT 或多鱼缸；
- 不加入温度校准偏移；
- 不改报警阈值或事件语义；
- 不控制加热棒、水泵、灯光或任何 220V 设备。
