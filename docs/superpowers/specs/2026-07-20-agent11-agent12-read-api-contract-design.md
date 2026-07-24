# Agent11 与 Agent12 鱼缸状态只读接口设计

**日期：** 2026-07-20  
**状态：** TZ 已批准接口方向，作为 Agent11 与后续 Agent12 的共同设计依据  
**机器契约：** `docs/contracts/fishtank-state-v1.schema.json`  
**共享样例：** `docs/contracts/fixtures/`

## 1. 文档目的

本文定义 Agent11 鱼缸监控系统与后续 Agent12 3D 数字孪生桌面端之间的唯一运行时
接口。目标是在真实 ESP32 硬件接入前允许两个项目独立开发，并在硬件、云端和桌面端
完成后以稳定契约联调。

本文只定义只读状态消费，不授权任何硬件控制能力。Agent12 不依赖 Codex 运行，不读取
Agent11 仓库，不连接 ESP32，也不理解 Agent11 的 C++ 内部状态。

## 2. 项目所有权与绝对边界

### 2.1 Agent11 所有权

Agent11 是状态生产者，负责：

- ESP32 双探头采样和温度事件状态机。
- 温度阈值、事件打开、升级、提醒和恢复的全部判断。
- ESP32 到腾讯云 SCF 的安全写入链路。
- COS 中最新设备状态的持久化。
- 设备在线、离线和未知状态的判定。
- 把内部字段标准化为本文定义的 `FishTankStateV1`。
- 提供只读 SCF `GET` 接口并保护私有 COS。

### 2.2 Agent12 所有权

Agent12 是状态消费者，负责：

- Electron、Vite、React、TypeScript 桌面应用。
- 通过 Electron 主进程轮询只读 HTTPS 接口。
- 使用 SWR 管理轮询、缓存和重试。
- 使用 Zustand 在 2D HUD 与 3D 场景间共享已校验状态。
- 显示温度、连接状态、数据时间和活跃告警。
- 对无数据、网络错误、协议错误和探头失效提供清晰的只读 UI。

### 2.3 Agent12 禁止事项

Agent12 及其依赖中不得存在以下产品能力：

- 向 Agent11、SCF、COS、ESP32 或其他硬件端点发起 `POST`、`PUT`、`PATCH` 或
  `DELETE`。
- 提交表单或发送任何控制命令。
- 修改告警阈值、采样频率、加热策略、水泵策略或设备配置。
- 在桌面端重新计算官方告警等级或官方在线状态。
- 保存腾讯云 SecretId、SecretKey、临时角色凭据或 ESP32 HMAC 密钥。
- 通过 React 渲染进程直接访问任意 URL 或构造任意 HTTP method。

Agent12 的 UI 可以控制纯本地展示行为，例如相机视角、3D 模型显隐、单位展示和窗口
布局。这些行为不得改变 Agent11 或物理鱼缸。

## 3. 已批准架构

```text
ESP32
  |
  | authenticated heartbeat
  v
Agent11 Tencent SCF writer
  |
  | private read/write access
  v
Private COS: devices/tank01/state.json
  |
  | server-side read
  v
Agent11 Tencent SCF read-only GET endpoint
  |
  | HTTPS GET + read-only Bearer token
  v
Agent12 Electron main process
  |
  | narrow preload IPC
  v
React + SWR + Zustand + R3F HUD
```

Agent12 不直接读取 COS。COS 保持私有，COS 内部对象结构不是 Agent12 的公共接口。
Agent11 可以重构 COS 字段，只要 SCF `GET` 接口继续返回兼容的 `FishTankStateV1`。

## 4. HTTP 接口

### 4.1 正式端点

```http
GET /api/v1/devices/{device_id}/state
```

Phase 1 唯一设备 ID：

```text
tank01
```

Agent12 使用运行时配置提供完整基础地址：

```text
FISHTANK_STATE_API_BASE_URL
FISHTANK_STATE_READ_TOKEN
FISHTANK_DEVICE_ID=tank01
```

这些值只能由 Electron 主进程读取。不得使用 `VITE_` 前缀把 Token 注入浏览器 bundle，
也不得通过 preload 暴露 Token。

### 4.2 允许的方法

- `GET`：读取最新状态。
- 其他所有方法：必须返回 `405 Method Not Allowed`。

Agent12 不得把 `OPTIONS`、`HEAD` 或写方法作为状态读取的降级路径。Phase 1 的读取发生
在 Electron 主进程，不依赖浏览器 CORS。

### 4.3 请求头

```http
Accept: application/json
Authorization: Bearer <read-only-token>
```

读 Token 与 ESP32 HMAC 密钥、腾讯云凭据完全独立。它只允许读取这一份有限状态，不具有
COS 写权限或设备控制权限。桌面客户端中的 Token 不能被视为不可提取的长期秘密，因此
服务端安全边界必须建立在“该 Token 只能读取”之上。

### 4.4 成功响应

```http
HTTP/1.1 200 OK
Content-Type: application/json; charset=utf-8
Cache-Control: no-store
```

响应体直接是 `FishTankStateV1`，不再包裹 `data`：

```json
{
  "schema_version": 1,
  "device_id": "tank01",
  "timestamp_ms": 1784557200000,
  "display_c": 26.4,
  "return_c": 26.6,
  "connectivity_status": "online",
  "events": []
}
```

### 4.5 错误响应

错误体使用稳定结构：

```json
{
  "ok": false,
  "error": "state_unavailable"
}
```

| HTTP 状态 | `error` | 语义 |
| --- | --- | --- |
| `400` | `invalid_device_id` | 路径中的设备 ID 不合法 |
| `401` | `unauthorized` | Token 缺失或无效 |
| `404` | `state_not_found` | 设备尚无任何有效云端状态 |
| `405` | `method_not_allowed` | 使用了 `GET` 以外的方法 |
| `503` | `state_unavailable` | COS 或 SCF 暂时不可用 |

错误响应不得包含 COS key、云端凭据、ESP32 密钥、Token、堆栈、内部异常或原始请求头。

## 5. TypeScript 数据契约

Agent12 应以以下类型作为领域边界：

```typescript
type TemperatureEventType =
  | 'high_temperature'
  | 'high_temperature_critical'
  | 'low_temperature'
  | 'low_temperature_critical'
  | 'sensor_fault'
  | 'temperature_rapid_change'
  | 'temperature_gradient';

type ActiveTemperatureEventState =
  | 'opened'
  | 'escalated'
  | 'reminder';

interface ActiveTemperatureEvent {
  type: TemperatureEventType;
  state: ActiveTemperatureEventState;
  severity: 'n2' | 'n3';
  at_ms: number;
  display_c: number | null;
}

interface FishTankStateV1 {
  schema_version: 1;
  device_id: string;
  timestamp_ms: number;
  display_c: number | null;
  return_c: number | null;
  connectivity_status: 'online' | 'offline' | 'unknown';
  events: ActiveTemperatureEvent[];
}

type FishTankSnapshot = FishTankStateV1 | null;
```

`FishTankSnapshot = null` 表示 Agent12 尚未取得任何通过校验的真实状态。Agent12 不得
用全零对象、假温度、当前本机时间或示例 fixture 冒充真实状态。

## 6. 字段级语义

### 6.1 `schema_version`

- Phase 1 固定为整数 `1`。
- Agent12 遇到未知主版本必须拒绝进入领域 Store，并显示协议不兼容。
- 不得猜测或静默转换未知主版本。

### 6.2 `device_id`

- Phase 1 固定为 `tank01`。
- 必须与请求路径中的设备 ID 一致。
- 仅允许小写字母、数字、下划线和连字符，长度 1 至 32。

### 6.3 `timestamp_ms`

- 表示 SCF 接收并接受最近一次有效 ESP32 状态心跳的服务器 Unix epoch 毫秒时间。
- 它不是 Agent12 的本地获取时间，也不是 Electron 启动时间。
- 它必须是非负安全整数。
- Agent11 使用服务器时间作为官方数据新鲜度依据，避免 Agent12 时钟影响官方状态。
- Agent12 可以显示数据年龄，但不得据此覆盖 `connectivity_status`。

### 6.4 `display_c`

- 主缸温度，单位为摄氏度。
- 有可信读数时为有限 JSON number。
- 没有接入真实数据、主缸探头失效或当前读数不可信时为 `null`。
- 禁止使用 `0`、旧值、空字符串、`NaN`、`Infinity` 或省略字段表达缺失。
- Agent12 看到 `null` 时必须显示无有效读数，不显示 `0°C`。

### 6.5 `return_c`

- 底滤回水温度，单位为摄氏度。
- 有可信读数时为有限 JSON number。
- 未接入、探头失效或读数不可信时为 `null`。
- `return_c` 不得被 Agent12 当作主缸安全温度的替代值。

### 6.6 `connectivity_status`

- `online`：Agent11 判定设备在线。
- `offline`：Agent11 判定设备超过已批准的离线时限。
- `unknown`：尚不足以形成在线或离线结论，或状态来自不完整迁移数据。

Agent11 是该字段的唯一权威生产者。Agent12 的网络请求失败属于“桌面端无法访问接口”，
不能自行把设备改判为 `offline`。

### 6.7 `events`

- 表示当前仍未恢复的完整事件快照，不是增量消息列表，也不是历史记录。
- 每次响应都必须包含数组；没有活跃事件时返回 `[]`。
- 同一个 `type` 最多出现一次。
- Agent12 每次成功读取后用整个数组替换旧数组，不做事件追加。
- Phase 1 不提供历史事件查询。

事件状态语义：

- `opened`：事件已经打开且尚未恢复。
- `escalated`：事件已经升级且尚未恢复。
- `reminder`：事件仍未恢复，最近一次输出是提醒。
- `resolved`：只存在于 Agent11 内部事件流，不得出现在此活跃快照中。

事件恢复后，Agent11 必须在下一份快照中移除该事件。Agent12 不等待单独的
`resolved` 消息，也不自行判断恢复。

### 6.8 事件 `severity`

- API 固定输出小写 `n2` 或 `n3`。
- Agent11 当前内部或既有传输若使用大写 `N2`、`N3`，必须在 API 投影层标准化。
- Agent12 不接受其他大小写，也不自行修改等级。

### 6.9 事件 `at_ms`

- 表示该活跃事件最近一次打开、升级或提醒的 Unix epoch 毫秒时间。
- 必须是非负安全整数。
- 它不是事件历史列表，也不保证等于事件最初打开时间。

### 6.10 事件 `display_c`

- `sensor_fault` 必须为 `null`。
- 其他事件通常为产生最近状态变化时的可信主缸温度。
- 如果 Agent11 无法提供可信值，必须为 `null`，不得使用旧值或伪造数值。
- Agent12 应优先依据事件类型和等级展示告警，不得用该值重新计算告警。

## 7. Agent11 状态投影要求

Agent11 可以保留内部 camelCase 字段、C++ enum 和云端持久化细节，但对外响应必须
确定性映射为本文契约。

当前实现与目标契约之间已知存在以下缺口：

| 当前状态 | 目标状态 |
| --- | --- |
| SCF HTTP 入口只处理心跳写入 | 增加独立只读 `GET` 路由 |
| COS 使用 `schemaVersion`、`deviceId`、`mainC` 等内部字段 | API 输出本文 snake_case 契约 |
| 当前 COS 没有活跃事件快照 | Agent11 输出完整 `events` |
| 当前心跳只发送有效双探头数字 | 必须支持无数据和探头失效的 `null` 语义 |
| 当前 API 没有桌面端读 Token | 增加独立只读 Bearer Token |

Agent11 必须通过升级心跳状态、维护云端状态投影或其他确定性方式获得完整活跃事件。
具体内部实现可以调整，但不得让 Agent12 从温度值反推事件。

Agent11 不得因为尚未有硬件数据而写入演示温度。第一份真实状态产生前，读取接口返回：

```http
404 state_not_found
```

## 8. Agent12 数据获取边界

推荐 Electron 边界：

```typescript
// preload 只暴露固定能力，不暴露 URL、Token 或通用 fetch。
window.fishtank.readState(): Promise<FishTankStateV1>;
```

主进程负责：

- 从运行时配置读取基础 URL、设备 ID 和只读 Token。
- 拼接固定 `GET` 路径。
- 设置 `Accept` 和 `Authorization`。
- 设置连接和响应超时。
- 解析 JSON 并按机器契约校验。
- 把通过校验的领域对象返回给 preload。

React/SWR 负责：

- 调用固定的 `window.fishtank.readState()`。
- 成功后把完整快照写入 Zustand。
- 初次成功前保持 `FishTankSnapshot = null`。
- 网络错误时保留最后一次成功快照，并单独显示接口访问异常。
- 不把 HTTP 错误解释为硬件离线。

推荐轮询值，不属于服务端兼容性契约：

- 默认轮询间隔：60 秒。
- 窗口重新聚焦：立即重新验证。
- 同一时刻只允许一个在途状态请求。
- 失败使用有上限的退避，不进行高频无限重试。

## 9. 数据校验与失败关闭

Agent12 必须在数据进入 Zustand 前进行运行时校验，至少检查：

- 响应是 JSON object。
- 所有必填字段存在，不接受隐式默认值。
- `schema_version === 1`。
- `device_id` 与配置一致。
- 所有时间为非负安全整数。
- 温度为 `null` 或有限数字。
- `connectivity_status`、事件类型、事件状态和等级属于枚举。
- 活跃事件中不含 `resolved`。
- `sensor_fault.display_c === null`。
- `events` 中不存在重复 `type`。

校验失败时：

- 不更新 Zustand 中最后一次通过校验的快照。
- 显示协议错误，而不是设备离线。
- 日志可以记录错误码和字段路径，但不得记录 Token 或完整请求头。

## 10. 版本兼容规则

以下改动兼容 `v1`：

- 增加 Agent12 明确忽略的顶层可选字段。既有字段的含义和类型不得改变。
- 修正文档但不改变字段含义。
- 服务端内部存储或实现变化。

以下改动必须发布 `/api/v2` 和 `schema_version: 2`：

- 删除或重命名必填字段。
- 改变字段类型、单位或 null 语义。
- 改变 `timestamp_ms` 的时间来源。
- 把活跃事件快照改为增量事件流。
- 改变 `connectivity_status` 的责任归属。
- 增加 Agent12 必须理解的新告警语义。
- 改变事件对象结构，或增加 Agent12 必须解释的事件字段。

Agent11 应在 Agent12 完成迁移前保留旧主版本。Agent12 不得静默适配未知主版本。

## 11. 开发期模拟场景

Agent12 在没有真实硬件时必须使用符合机器契约的本地 fixtures。fixture 只能进入开发和
测试环境，生产构建不得自动回退到模拟数据。

Agent11 提供以下共享样例，Agent12 应在自身仓库中保留来源和契约版本：

- `fishtank-state-v1.normal.json`
- `fishtank-state-v1.sensor-fault.json`
- `fishtank-state-v1.offline.json`
- `fishtank-state-v1.invalid-resolved.json`，用于证明客户端会拒绝非法快照

至少覆盖：

1. 接口返回 `404 state_not_found`，Store 保持 `null`。
2. 双探头正常、设备在线、无活跃事件。
3. 主缸温度为 `null` 且存在 `sensor_fault`。
4. N2 活跃事件。
5. N3 升级事件。
6. 多个不同类型活跃事件。
7. 设备被 Agent11 判定为 `offline`，同时保留最后可信温度。
8. HTTP 超时、`401`、`503` 和非 JSON 响应。
9. 未知 schema 版本。
10. 重复事件类型、非法枚举、`resolved` 混入快照和伪造 `0°C`。

正常状态示例：

```json
{
  "schema_version": 1,
  "device_id": "tank01",
  "timestamp_ms": 1784557200000,
  "display_c": 26.4,
  "return_c": 26.6,
  "connectivity_status": "online",
  "events": []
}
```

探头故障示例：

```json
{
  "schema_version": 1,
  "device_id": "tank01",
  "timestamp_ms": 1784557500000,
  "display_c": null,
  "return_c": 26.5,
  "connectivity_status": "online",
  "events": [
    {
      "type": "sensor_fault",
      "state": "opened",
      "severity": "n2",
      "at_ms": 1784557500000,
      "display_c": null
    }
  ]
}
```

离线状态示例：

```json
{
  "schema_version": 1,
  "device_id": "tank01",
  "timestamp_ms": 1784557200000,
  "display_c": 26.4,
  "return_c": 26.6,
  "connectivity_status": "offline",
  "events": []
}
```

离线不等于温度无效。Agent11 可以保留最近一次可信读数，同时明确标记设备离线；
Agent12 必须同时展示数据年龄，避免把历史温度表现为实时温度。

## 12. 双方契约测试

Agent11 完成接口时至少验证：

- 只读 Token 正确时 `GET` 返回通过 JSON Schema 的状态。
- Token 缺失或错误返回 `401`，且不读取或输出敏感信息。
- 所有非 `GET` 方法返回 `405`，且不写 COS。
- 设备尚无状态时返回 `404`，不生成假数据。
- 私有 COS 字段正确标准化为 API 字段。
- 事件恢复后从下一份快照移除。
- `sensor_fault` 与无可信温度都输出 `null`。
- 当前大写等级在 API 层标准化为小写。

Agent12 完成客户端时至少验证：

- 生产网络模块只能发起 `GET`。
- preload 不暴露 Token、URL 或通用网络调用。
- 所有开发 fixtures 通过同一 JSON Schema。
- 未通过校验的响应不会进入 Zustand。
- 初始无数据状态为 `null`。
- 网络错误保留最后一次有效快照，并与设备离线分开显示。
- 3D 场景和 HUD 只消费 Zustand，不各自重复请求后端。
- 生产构建不存在自动模拟数据降级。

## 13. 首次真实联调验收

硬件接入后按以下顺序验收：

1. ESP32 产生第一份真实状态，接口从 `404` 转为 `200`。
2. Agent12 显示两个真实温度、服务器时间和 `online`。
3. 拔除主缸探头，确认 `display_c: null` 和 `sensor_fault`。
4. 恢复主缸探头，确认事件从快照移除且温度重新有效。
5. 用受控测试触发 N2、N3、reminder 和 resolved 闭环。
6. 停止心跳超过已批准离线阈值，确认 Agent11 输出 `offline`。
7. 恢复心跳，确认 Agent11 输出 `online`。
8. 暂时阻断 Agent12 到 SCF 的网络，确认 UI 表示接口异常而非伪造硬件离线。
9. 使用错误 Token 和写方法，确认分别返回 `401` 和 `405`。
10. 检查 SCF、Electron 和 UI 日志中没有任何密钥或完整 Authorization header。

只有上述验收完成，才能声明 Agent11 与 Agent12 的真实数据链路完成。3D 界面能显示
fixture、COS 中存在对象或单次 `GET` 成功，都不足以单独证明端到端联调完成。

## 14. 非目标

Phase 1 不包括：

- 历史曲线和长期事件存档。
- WebSocket、MQTT 或服务端推送到桌面端。
- 多鱼缸管理。
- 用户账号体系和远程多租户授权。
- 桌面端修改阈值或设备配置。
- 加热棒、水泵、插排、灯光、喂食或其他硬件控制。
- Agent12 对 Agent11 告警算法的复制或替代。

后续如需新增上述能力，必须单独设计、审批并发布新的契约版本，不能在 `v1` 中静默扩展
控制能力。
