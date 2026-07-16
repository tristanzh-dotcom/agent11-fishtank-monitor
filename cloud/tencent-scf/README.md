# 腾讯云 SCF 安全模式（MVP）

本目录提供两个 Node.js 20 云函数入口，共用一个部署包：

- `heartbeat.main_handler`：接收 ESP32 心跳，鉴权后覆盖写入固定 COS 对象。
- `offline.main_handler`：由 5 分钟定时器触发，只在离线/恢复状态切换时发送 Bark。

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

部署包只包含两个入口、`src/`、锁定的生产依赖和包元数据，不包含测试、README、
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

`fishtank-heartbeat`：

- `COS_BUCKET=fishtank-monitor-1454792551`
- `COS_REGION=ap-shanghai`
- `DEVICE_SECRETS_JSON`：例如一个仅包含 `tank01` 的 JSON 对象；不要写入代码或聊天。

`fishtank-offline-checker`：

- `COS_BUCKET=fishtank-monitor-1454792551`
- `COS_REGION=ap-shanghai`
- `DEVICE_IDS=tank01`
- `OFFLINE_AFTER_MS=900000`
- `BARK_KEY`：在控制台私下输入，不写入代码、URL 或聊天。

COS SDK 只读取腾讯 SCF 运行角色注入的临时凭据：
`TENCENTCLOUD_SECRETID`、`TENCENTCLOUD_SECRETKEY`、
`TENCENTCLOUD_SESSIONTOKEN`。不要创建或配置长期 SecretId/SecretKey。

## 腾讯云配置硬约束

两个函数都选择上海区、Node.js 20、128 MB 内存；关闭日志投递，不开通 CLS，不配置
预置并发，最大独占配额设置为 128 MB。超时均设置为 3 秒。

- 心跳函数：先创建但不启用公网触发器；最终使用函数 URL，入口
  `heartbeat.main_handler`。
- 离线函数：先创建但不启用定时器；最终每 5 分钟运行一次，入口
  `offline.main_handler`。
- 运行角色：仅允许对
  `fishtank-monitor-1454792551/devices/*/state.json` 执行 COS GetObject/PutObject。

上传部署包、输入 Bark/设备密钥、启用公网 URL 和启用定时器是四个独立确认点。
