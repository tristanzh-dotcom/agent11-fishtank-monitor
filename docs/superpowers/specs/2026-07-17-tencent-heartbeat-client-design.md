# 腾讯云心跳客户端与在线状态闭环设计

## 目标

在不改变现有温度报警策略的前提下，为第一缸 `tank01` 增加设备在线状态监控：

`ESP32-S3 → 腾讯云 SCF 函数 URL → COS 固定状态对象`

SCF 每 5 分钟由定时触发器检查最后一次有效心跳；15 分钟没有心跳时通过 Bark
发送离线通知，恢复心跳后只发送一次恢复通知。

直接 HTTPS Bark 仍是温度报警主链路。腾讯云心跳是独立的附加链路，失败不得阻塞
温度采样、报警判断、Bark 告警或可选 MQTT。

## 固定边界

- 设备：`tank01`。
- 区域：上海 `ap-shanghai`。
- COS：`fishtank-monitor-1454792551`。
- 状态对象：`devices/tank01/state.json`。
- SCF：`default/fishtank-monitor`，Node.js 20、128 MB、执行超时 10 秒。真实冷启动
  加首次 COS 写入可能超过 3 秒。
- SCF 不启用 CLS、APM、VPC、文件系统或预置并发。
- 公网入口只接受 HMAC-SHA256 签名的 1 KiB 以内 JSON。
- 运行角色只允许对上述单个 COS 对象执行 `GetObject` 和 `PutObject`。
- 不接入或控制加热棒、水泵、灯具及任何 220 V 设备。

## 设备端行为

### 调度

- Wi-Fi 可用后配置 UTC 时间同步，优先 `pool.ntp.org`，备用
  `time.cloudflare.com`。
- 只有取得合理的 Unix 时间并且最近一次双探头采样有效时才发送心跳。
- 正常周期为 5 分钟。发送失败使用独立指数退避，初始 5 秒、上限 5 分钟。
- 每次尝试使用最新温度，不保存历史心跳队列；云端只需要最新在线状态。
- 温度事件先进入并处理 Bark/MQTT 队列，心跳发送排在其后。

### 请求

请求体字段固定为：

```json
{
  "device_id": "tank01",
  "sent_at_ms": 1750000000000,
  "nonce": "16至64位十六进制随机数",
  "main_c": 26.4,
  "sump_c": 26.6,
  "uptime_ms": 123456
}
```

设备对实际发送的原始 JSON 计算 SHA-256，再对下列 canonical string 计算
HMAC-SHA256：

```text
v1
<sent_at_ms>
<nonce>
<sha256(raw JSON body)>
```

签名以小写十六进制写入 `X-Aquarium-Signature`。nonce 每次发送重新使用
`esp_random()` 生成，不复用失败请求的 nonce。签名、共享密钥、函数 URL 和完整请求体
不得写入串口或日志。

### TLS 与秘密

- 使用 `WiFiClientSecure` 校验函数 URL 的 TLS 证书；禁止 `setInsecure()`。
- 函数 URL、设备共享密钥和根证书只放入被 Git 忽略的
  `include/secrets.hpp`。
- 仓库只提供无效占位模板，不保存真实 Bark Key、设备密钥或函数 URL。
- 腾讯云端设备密钥只放在 SCF 环境变量 `DEVICE_SECRETS_JSON`。
- SCF 访问 COS 使用运行角色临时凭据，不配置长期 SecretId/SecretKey。
- 非镜像 Node.js 运行时从 `TENCENTCLOUD_SECRETID`、
  `TENCENTCLOUD_SECRETKEY`、`TENCENTCLOUD_SESSIONTOKEN` 环境变量读取角色临时凭据，
  不从事件 `context` 读取。

## 云端行为

- Function URL 验证设备 ID、时间偏差、nonce、请求大小和 HMAC 后，覆盖唯一状态对象。
- 相同 nonce 不得重复接受。
- Timer 每 5 分钟读取状态对象：
  - 首次超过 15 分钟未更新：写入离线状态，Bark 通知一次；
  - 继续离线：不重复通知；
  - 离线后收到有效心跳：记录待恢复；
  - 下一次 Timer：发送一次恢复通知并写回健康状态。
- Bark 失败时不提前提交通知状态，以便下次 Timer 重试。
- 对公网 HTTP 响应设置至少 500 ms 的固定下限，且不返回签名、密钥或 COS 凭据。

## 无硬件验收

- 主机测试锁定 JSON、canonical string、nonce、周期和退避行为。
- PlatformIO 完整编译证明 ESP32-S3 固件与 mbedTLS/HTTPS 集成可构建。
- Node 测试锁定云函数和签名模拟器行为。
- 使用软件模拟设备向真实函数 URL 发送有效和无效请求。
- 在真实 COS 中确认固定对象被覆盖，不创建按时间增长的全量日志。
- 人工触发或受控调整状态完成离线、去重和恢复 Bark 闭环。

2026-07-17 已完成全部无硬件云端验收：有效签名 200、重放 409、无效签名 401；
固定 COS 对象写入成功且无效 nonce 未落盘。`BARK_KEY` 已由用户私下输入，五分钟
Timer `fishtank-monitor-offline-check` 已启用；受控状态验证了首次离线、重复离线
no-op、有效心跳恢复、首次恢复及重复恢复 no-op。COS 状态只在 Bark 成功后提交，
因此离线与恢复 Bark API 各成功接受一次。

## 硬件边界

以下项目必须等开发板和探头到货：

- 烧录真实 ESP32-S3 并验证 Wi-Fi/NTP、TLS、HMAC 与真实函数 URL。
- 写入最终设备共享密钥并完成首次真机心跳。
- 验证断电超过 15 分钟的真实离线通知和重新上电恢复通知。
- 进行双探头校准、桌面运行和鱼缸 48–72 小时观察。
