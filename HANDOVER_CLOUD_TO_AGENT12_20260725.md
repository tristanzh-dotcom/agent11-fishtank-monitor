# Agent11 -> Agent12 云端接入交接

日期：2026-07-25
项目：鱼缸温度监控 MVP / 数字孪生鱼缸

## 1. 当前结论

云服务端开发阶段已完成，当前进入验收测试阶段。最新腾讯云 SCF 版本已经部署到：

- 区域：上海
- 命名空间：`default`
- 函数：`fishtank-monitor`
- 运行环境：Node.js 20.19
- 入口：`monitor.main_handler`
- Function URL：已启用 HTTPS 公网访问
- COS：私有存储，仅保存 `devices/tank01/state.json`
- 定时器：5 分钟离线检查

用户可见名称已经改为“包包大缸”。内部设备 ID 仍然是 `tank01`，不能在 Agent12
接入时改成中文；COS key、心跳协议和 API 路径都继续使用 `tank01`。

## 2. Agent12 读取接口

请求：

```text
GET /api/v1/devices/tank01/state
Authorization: Bearer <STATE_READ_TOKEN>
```

令牌要求：

- 仅从本机钥匙串或 Agent12 的安全配置读取；
- 不写入仓库、前端代码、URL、日志、截图或聊天；
- 不与 ESP32 HMAC 设备密钥或 Bark key 复用。

接口输出为 `FishTankStateV1` 投影，不暴露 COS 内部字段、设备密钥、Bark key 或云函数
临时凭据。字段：

```json
{
  "schema_version": 1,
  "device_id": "tank01",
  "timestamp_ms": 0,
  "display_c": 27.8,
  "return_c": 27.8,
  "connectivity_status": "online",
  "events": []
}
```

字段映射：

- `display_c`：主缸温度，Agent12 显示为“主缸”；
- `return_c`：底滤缸/回水侧温度，Agent12 显示为“底滤缸”；
- `connectivity_status`：`online`、`offline` 或 `unknown`；
- `events`：当前活动事件快照，不是历史日志。

事件类型包括：

- `high_temperature`
- `high_temperature_critical`
- `low_temperature`
- `low_temperature_critical`
- `temperature_rapid_change`
- `temperature_gradient`
- `sensor_fault`
- `network_offline`
- `network_recovered`
- `heartbeat_missing`

事件状态包括：`opened`、`escalated`、`reminder`。`display_c` 为 `null` 时不得在数字孪生中
伪造温度或使用旧值冒充当前值；`sensor_fault` 的 `display_c` 必须为 `null`。

## 3. 错误契约

Agent12 应区分以下响应，不要把所有错误显示为“设备离线”：

- `200`：状态读取成功；
- `401 unauthorized`：令牌缺失或错误；
- `404 state_not_found`：设备尚无有效云端状态；
- `405 method_not_allowed`：使用了非 GET 方法；
- `503 state_unavailable`：COS 或状态投影暂时不可用。

网络请求失败应显示为客户端网络错误，并保留云端最后一次已验证状态的边界；不要伪造
新的实时读数。

## 4. 当前已验证内容

- SCF Node.js 测试：`40/40` 通过；
- ESP32-S3 固件构建成功并已刷写；
- 两支 DS18B20 已完成桌面稳定性验证；
- 安装到鱼缸后主缸/底滤缸均有真实读数；
- 真实高温事件已通过 ESP32 -> SCF -> Bark 手机链路验证；
- Bark 文案已覆盖高温、低温、严重等级、快速变化、缸体温差、传感器故障、离线和恢复；
- 用户可见设备名为“包包大缸”。

## 5. 尚未完成的验收

这些不是 Agent12 接入的前置开发阻塞，但应在联合验收时覆盖：

1. 只读 API 的真实 `200/401/404/405` 外部请求验收；
2. 低温、快速变化、温差异常、传感器故障的真实 Bark 场景；
3. 告警去重、N2 -> N3 升级和 Bark 失败重试的云端实测；
4. 最终独立供电 48-72 小时观察。

长时间观察不是 Agent12 开发的前置条件。Agent12 可以先使用真实云端状态接口完成数字孪生
渲染、事件状态映射和错误态展示。

## 6. 相关代码与文档

- 云函数入口：`cloud/tencent-scf/monitor.js`
- SCF 路由：`cloud/tencent-scf/src/scf_runtime.mjs`
- 只读 API：`cloud/tencent-scf/src/state_read_handler.mjs`
- 心跳处理：`cloud/tencent-scf/src/heartbeat_handler.mjs`
- Bark 文案：`cloud/tencent-scf/src/bark_notifier.mjs`
- 状态契约：`docs/contracts/fishtank-state-v1.schema.json`
- 云服务说明：`cloud/tencent-scf/README.md`

## 7. 安全边界

Agent12 只读访问 Function URL，不接触 COS 密钥、SCF 角色凭据、ESP32 HMAC 设备密钥或
Bark key。不要把任何秘密放入 3D 场景、浏览器渲染结果、录屏、日志或提交记录。

