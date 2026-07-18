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

请求必须是 `POST application/json`，解码后不超过 1024 字节：

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

请求头 `X-Aquarium-Signature` 是 64 位十六进制 HMAC-SHA256。签名原文为：

```text
v1\n<sent_at_ms>\n<nonce>\n<sha256(原始 JSON 请求体)>
```

服务端允许设备时间与云端时间相差最多 5 分钟。设备密钥至少 16 字符，正式使用建议
在本机执行 `openssl rand -hex 32` 生成，并只在 ESP32 私有配置和 SCF 环境变量中输入。

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
- `DEVICE_SECRETS_JSON`：例如一个仅包含 `tank01` 的 JSON 对象；不要写入代码或聊天。
- `DEVICE_IDS=tank01`
- `OFFLINE_AFTER_MS=900000`
- `BARK_KEY`：在控制台私下输入，不写入代码、URL 或聊天。

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
  `fishtank-monitor-1454792551/devices/tank01/state.json` 执行 COS
  GetObject/PutObject。

Bark 采用“提醒优先”的至少一次投递：若 Bark 已成功但紧随其后的 COS 状态写入失败，
下一轮可能重复同一条状态切换提醒；这比漏掉离线报警更安全。

上传部署包、输入 Bark/设备密钥、启用公网 URL 和启用定时器是四个独立确认点。

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
