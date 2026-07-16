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

## 云函数环境变量

单一函数 `fishtank-monitor` 配置以下全部变量：

- `COS_BUCKET=fishtank-monitor-1454792551`
- `COS_REGION=ap-shanghai`
- `DEVICE_SECRETS_JSON`：例如一个仅包含 `tank01` 的 JSON 对象；不要写入代码或聊天。
- `DEVICE_IDS=tank01`
- `OFFLINE_AFTER_MS=900000`
- `BARK_KEY`：在控制台私下输入，不写入代码、URL 或聊天。

COS SDK 每次调用都只读取腾讯 SCF `context` 注入的最新临时凭据：
`TENCENTCLOUD_SECRETID`、`TENCENTCLOUD_SECRETKEY`、
`TENCENTCLOUD_SESSIONTOKEN`，不会缓存环境变量中的冷启动凭据。不要创建或配置长期
SecretId/SecretKey。

## 腾讯云配置硬约束

只创建一个 `fishtank-monitor` 函数，选择上海区、Node.js 20、128 MB 内存；关闭日志
投递，不开通 CLS，不配置预置并发，最大独占配额设置为 128 MB，超时设置为 3 秒。
COS 存储桶必须复核为私有读写、单可用区、SSE-COS、版本控制关闭。

- 执行入口：`monitor.main_handler`。
- 函数 URL：先不启用；最终作为 ESP32 心跳入口。
- 定时器：先不启用；最终每 5 分钟运行一次，事件类型必须为 `Timer`。
- 运行角色：仅允许对
  `fishtank-monitor-1454792551/devices/*/state.json` 执行 COS GetObject/PutObject。

Bark 采用“提醒优先”的至少一次投递：若 Bark 已成功但紧随其后的 COS 状态写入失败，
下一轮可能重复同一条状态切换提醒；这比漏掉离线报警更安全。

上传部署包、输入 Bark/设备密钥、启用公网 URL 和启用定时器是四个独立确认点。
