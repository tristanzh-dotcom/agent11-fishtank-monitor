# 腾讯云 SCF 安全模式（MVP）

本目录提供一个 Node.js 20 云函数入口 `monitor.main_handler`，同时接收两类事件：

- 函数 URL：接收 ESP32 心跳，鉴权后覆盖写入固定 COS 对象。
- 5 分钟定时器：只在离线/恢复状态切换时发送 Bark。

必须让两个触发器指向同一个函数，并将 128 MB 函数的最大独占配额设为 128 MB。
这样云端全局最多只有一个并发实例；入口内部也会串行处理调用，避免定时检查用旧状态
覆盖刚收到的心跳。不要拆成两个函数。

运行时不写应用日志、不依赖 CLS，也不创建持续增长的历史对象。每台设备只使用
`devices/<deviceId>/state.json` 一个固定 COS key；状态中的 nonce 历史最多保留 16 个。

## 本地验证

```bash
cd cloud/tencent-scf
npm ci --ignore-scripts --no-audit --no-fund
npm audit --omit=dev
npm test
```

生成或原子替换仓库根目录 `.build/` 中的上传包：

```bash
npm run build
```

部署包只包含入口、串行器、`src/`、锁定的生产依赖和包元数据，不包含测试、
本地配置、固件密钥或 Bark key。

## 心跳协议

请求必须是 `POST application/json`，解码后不超过 2,048 字节：

```json
{
  "device_id": "esp1",
  "sent_at_ms": 1750000000000,
  "nonce": "16至64位十六进制随机数",
  "main_c": 26.4,
  "sump_c": 26.6,
  "uptime_ms": 123456,
  "active_events": []
}
```

新固件可在同一心跳中附带私有五缸摘要：

```json
{
  "temperature_snapshot": {
    "sampled_at_ms": 1750000000000,
    "summary_text": "采样时间：..."
  }
}
```

`sampled_at_ms` 必须是正的安全整数，`summary_text` 必须是非空 UTF-8 文本且不超过
768 字节；字段仍计入 2,048 字节心跳上限和原始 JSON HMAC。固件在可选摘要超限时省略
整个字段并继续发送原有心跳，云端在收到不合法字段时拒绝该心跳。

请求头 `X-Aquarium-Signature` 是 64 位十六进制 HMAC-SHA256。签名原文为：

```text
v1\n<sent_at_ms>\n<nonce>\n<sha256(原始 JSON 请求体)>
```

服务端允许设备时间与云端时间相差最多 5 分钟。设备密钥至少 16 字符，正式使用建议
在本机执行 `openssl rand -hex 32` 生成，并只在 ESP32 私有配置和 SCF 环境变量中输入。

`main_c`、`sump_c` 和每个事件的 `display_c` 都可以是 JSON `null`；缺测时绝不能用
旧读数或 `0` 伪造。`active_events` 最多七项，按事件类型去重；`sensor_fault` 的
`display_c` 必须为 `null`。心跳签名原文仍为 `v1`，并覆盖包含这些字段的原始请求体。

成功心跳会覆盖私有 COS 对象 `devices/esp1/state.json`，内部状态版本为 2。该对象
不是公开 API，且不会用作历史日志。

## 只读状态 API

同一函数 URL 提供下游消费者使用的只读接口：

```text
GET /api/v1/devices/esp1/state
Authorization: Bearer <STATE_READ_TOKEN>
```

它只读取 COS 并投影为 `FishTankStateV1`，字段为 `schema_version`、`device_id`、
`timestamp_ms`、`display_c`、`return_c`、`connectivity_status` 和 `events`。它不会暴露
COS 内部字段、HMAC 设备密钥、Bark key 或云函数临时凭据。令牌比较使用常量时间比较；
令牌长度必须为 32–256 个字符，且必须与设备 HMAC 密钥不同。错误保持为不含敏感信息的
`401 unauthorized`、`404 state_not_found`、`405 method_not_allowed` 或
`503 state_unavailable`。

远程五缸快捷查询复用同一个只读令牌和函数 URL：

```text
GET /api/v1/devices/esp1/temperature-summary
Authorization: Bearer <STATE_READ_TOKEN>
```

该路径返回 `text/plain; charset=utf-8` 并设置 `Cache-Control: no-store`。新固件通过
可选的 `temperature_snapshot` 心跳字段上传最新日报正文；旧固件省略该字段时仍可正常
处理心跳，摘要接口返回“暂无温度数据，请稍后重试。”。设备离线或采样超过十分钟时，
接口只返回对应中文提示，不展示旧温度；现有 `/state` JSON 投影保持不变。该接口只读，
不会写 COS、刷新在线时间或触发 Bark。

软件模拟设备可用于硬件到货前的真实函数 URL 验证。变量只在当前终端进程中提供，
脚本输出仅包含 HTTP 状态，不打印 URL、密钥、nonce 或签名：

```bash
FISHTANK_FUNCTION_URL='私有输入' \
FISHTANK_DEVICE_SECRET='私有输入' \
npm run simulate-heartbeat
```

## 云函数环境变量

单一函数 `fishtank-monitor` 配置以下全部变量：

- `COS_BUCKET=fishtank-monitor-1454792551`
- `COS_REGION=ap-shanghai`
- `DEVICE_SECRETS_JSON`：例如一个仅包含 `esp1` 的 JSON 对象；不要写入代码或聊天。
- `DEVICE_IDS=esp1`
- `OFFLINE_AFTER_MS=900000`
- `BARK_KEY`：在控制台私下输入，不写入代码、URL 或聊天。
- `STATE_READ_TOKEN`：32–256 字符、独立于 `DEVICE_SECRETS_JSON` 的只读令牌；仅在
  控制台私下输入，不能写入仓库、URL、聊天或设备固件。

COS SDK 每次调用都只读取腾讯 SCF 运行环境注入的最新临时凭据环境变量：
`TENCENTCLOUD_SECRETID`、`TENCENTCLOUD_SECRETKEY`、
`TENCENTCLOUD_SESSIONTOKEN`，不会从事件 `context` 读取凭据，也不会配置或缓存长期
SecretId/SecretKey。

## 腾讯云配置硬约束

只创建一个 `fishtank-monitor` 函数，选择上海区、Node.js 20、128 MB 内存；关闭日志
投递，不开通 CLS，不配置预置并发，最大独占配额设置为 128 MB，执行超时设置为
10 秒。实测 Node.js 冷启动加首次 COS 写入可能超过 3 秒，因此不能继续使用 3 秒上限。
COS 存储桶必须复核为私有读写、单可用区、SSE-COS、版本控制关闭。

- 执行入口：`monitor.main_handler`。
- 函数 URL：已启用 HTTPS 公网入口，由应用层 HMAC 鉴权。
- 定时器：已启用 `fishtank-monitor-offline-check`，每 5 分钟运行一次，事件类型为
  `Timer`，Cron 为 `0 */5 * * * * *`。
- 运行角色：仅允许对
  `fishtank-monitor-1454792551/devices/esp1/state.json` 执行 COS
  GetObject/PutObject。

Bark 采用“提醒优先”的至少一次投递：若 Bark 已成功但紧随其后的 COS 状态写入失败，
下一轮可能重复同一条状态切换提醒；这比漏掉离线报警更安全。

云端主缸温度通知与设备直推采用同一顺序的双时间正文：先显示事件读数，再显示判定时间、
本次云端发送发起时间和北京时间标识。现有心跳只携带设备单调运行时间，不能可信转换为日历
事件时间，因此该栏固定显示“事件时间不可用”；离线/恢复通知保持原文案。

上传部署包、输入 Bark/设备密钥、启用公网 URL 和启用定时器是四个独立确认点。
本次新版部署包已按授权发布；`STATE_READ_TOKEN` 不随代码包上传，继续由现有私下配置提供。
之后仍需真机刷写、Wi-Fi/NTP、真实心跳、Bark 与下游读取验收。

## 2026-07-17 云端验收记录

- 修复后的运行时已部署到上海区 `default/fishtank-monitor`。
- 软件模拟的有效签名心跳返回 HTTP 200。
- 同一有效请求再次发送返回 HTTP 409 `replay_detected`。
- 无效签名返回 HTTP 401 `request_rejected`。
- COS 中仅创建固定对象 `devices/tank01/state.json`；验收时为 278B，并保存
  `tank01` 的 26.4/26.6°C、在线状态和单次有效 nonce。
- 无效签名的 nonce 未写入状态对象。
- `BARK_KEY` 已由用户在腾讯云控制台私下输入并以掩码显示；未写入代码或验收输出。
- 五分钟 Timer 已启用。受控状态验收完成首次离线、重复离线不提醒、有效心跳恢复、
  首次恢复提醒和重复恢复不提醒。
- 离线与恢复状态都只在 Bark 请求成功后写回 COS；验收状态已写回，证明 Bark API
  各接受一次离线和恢复通知，后续重复 Timer 均为 no-op。
- 当前模拟设备密钥只用于无硬件验收；真机烧录前必须轮换，并仅同步到腾讯云环境变量
  与本机忽略文件。
