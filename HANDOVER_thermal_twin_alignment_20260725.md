### ☀️ 次日启动胶囊 (Boot Prompt)

请在明天开启新对话时，直接复制以下指令发给系统：

```text
请静默读取并完全理解当前目录下的 `HANDOVER_thermal_twin_alignment_20260725.md`。

1. 请将本对话的逻辑分支锁定为：【Agent11-Agent12 数字孪生真实联调】，并在你回复的第一句话使用 Markdown 的 H1 标题 (`# Agent11-Agent12 数字孪生真实联调 工作流重启`) 输出，以便系统自动重命名此对话。
2. 请在执行任何操作前，简要复述当前的【核心卡点】与【下一步行动】。
3. 请在回复末尾明确提示：『请 TZ 确认：是按计划执行，还是需要进行微调？』
4. 在收到 TZ 明确授权前，不得执行文件修改、命令运行、方案推进或任务范围扩展；仅允许复述理解、指出不确定项并等待确认。
```

## 1. 第一性原理与项目上下文

### 当前项目根

```text
/Users/tristanzh/agent/agent11-fishtank-monitor
```

### 当前工作流主题

```text
Agent11-Agent12 数字孪生真实联调
```

### 核心目的

Agent11 负责真实双探头温度采集和温度事件判断；腾讯云 SCF/COS 负责在线状态和只读状态投影；Agent12 负责将已验证状态转换为脱敏公开快照并驱动 Kimi-K3 完成的 3D 数字孪生。

### 当前上下文边界

- 本交接覆盖真实 ESP32-S3、COS 状态、Agent11 GET、Agent12 upstream/public 状态和 cartoon-tank 真实消费的一致性。
- Kimi-K3 负责 3D 模型和场景表现；不得重做几何、场景树或运动学。
- 不修改温度策略、SCF 协议或任何 220V/继电器/SSR/加热棒/水泵控制。
- 不输出或写入任何 Token、设备密钥、Bark key、COS 凭据或真实 Function URL。

## 2. 今日完成事项

### 修改文件清单

```text
- Agent11 代码：今日未确认有核心实现文件修改。
- Agent12：已完成工作流对齐消息发送；现有热状态投影和视觉代码未在本线程重复修改。
```

### 已完成工作

- 已定位并读取“3D数字孪生鱼缸v2”工作流线程。
- 已向该工作流发送真实 COS 状态、字段映射、已通过验证和安全边界。
- 已要求对方优先完成真实 GET `200`、upstream health、`last_success_at_ms`、公开状态和 3D 页面联调。
- 已明确温差事件只做诊断，不将底滤误判为 high/low。
- 已明确当前字段链路：

  ```text
  串口 main_tank/sump_tank
    -> COS mainC/sumpC
    -> Agent11 GET display_c/return_c
    -> Agent12 thermal_state main/sump
  ```

## 3. 已作出的关键决策

```text
- 决策：Agent12 继续只读消费已部署 SCF GET，不修改 SCF 或 ESP32 策略。
- 原因：云端接口和设备端事件判断已形成批准边界，真实联调应验证边界而不是扩大协议。
- 放弃方案：不使用 fixture、手工 COS 状态或模拟心跳替代真实 GET/页面证据。
- 证据来源：HANDOVER_CLOUD_TO_AGENT12_20260725.md、用户提供的真实 COS v2 快照、Agent12 工作流对齐消息。

- 决策：真实状态中的 low_temperature_critical 只投影为主缸 low/N3；temperature_gradient 不改变 sump 热状态。
- 原因：底滤没有独立方向性温度事件字段，不能从诊断事件或温度数值自行猜测用户异常方向。
- 放弃方案：根据底滤温度或温差事件在前端重新判断阈值。
- 证据来源：Agent11 FishTankStateV1 合同和现有 projectThermalState 实现。
```

## 4. 验证状态

### 已运行命令

```text
本次收工动作未运行额外测试。
```

### 已有验证结果

```text
- Agent12 web-service：145/145 测试通过，TypeScript 检查通过。
- Agent12 cartoon-tank：22/22 测试通过，TypeScript 检查通过，生产构建成功。
- 真实 COS v2 状态已只读投影为 main low/N3、sump normal/0。
```

### 未验证项与手动验证路径

- 当前线程没有绑定真实 GET 会话，尚未取得 Agent11 GET `200` 的直接外部响应证据。
- 不得把 COS 内部 JSON 当作 GET 响应证据。
- 下一次应在已授权的安全凭据环境中执行只读 GET，核对状态码、snake_case 字段、事件枚举和 `null` 语义；再核对 Agent12 `8012` upstream health、公开状态和 3D 页面显示。

## 5. 未解决的风险/报错

```text
- 风险：真实 GET 200 尚未在当前线程取得。
  当前状态：Agent12 工作流已启动并有并行子任务，但仍在进行真实联调。
  影响范围：无法宣称 Agent11 GET 到页面的持续生产闭环已完成。
  建议下一步：先取得只读 GET 证据，再进行页面和事件矩阵验证。

- 风险：stale、恢复、sensor_fault、高温、物理网络断开/恢复尚未全部取得真实现场证据。
  当前状态：当前只确认真实低温事件和在线状态。
  影响范围：真实事件闭环仍未完成。
  建议下一步：按事件门禁逐项验证，不用 fixture 冒充真实证据。

- 风险：Agent12 工作树存在其他未提交改动。
  当前状态：对齐时未执行 Git 写操作，也未回滚任何改动。
  影响范围：后续修改必须避免覆盖其他工作流变化。
  建议下一步：先读取 Agent12 最新交接和线程状态，再决定是否需要最小修复。
```

## 6. 下一步行动 (原子化设计)

- [ ] **Action-1 (真实接口)**：在安全凭据环境中对 Agent11 GET 执行一次只读请求；预期获得 `200` 或明确的 `401/404/503`，不得输出 Token。
- [ ] **Action-2 (合同核对)**：校验响应字段 `schema_version/device_id/timestamp_ms/display_c/return_c/connectivity_status/events`，确认 `main_tank/sump_tank` 不被错误写入公共 API。
- [ ] **Action-3 (服务状态)**：核对 Agent12 upstream health、`last_success_at_ms`、公开快照和 stale 状态；区分真实 offline 与客户端网络错误。
- [ ] **Action-4 (页面证据)**：在不暴露凭据的前提下确认 cartoon-tank HUD 温度和 `thermal_state` 来自同一份快照，主缸/底滤分区状态一致。
- [ ] **Action-5 (事件矩阵)**：继续真实验证 stale、恢复、sensor_fault、低温、高温、网络断开/恢复；每项记录实际输入、输出和未完成门禁。
- [ ] **Action-6 (回归)**：任何代码修改后分别运行 Agent12 web-service 与 cartoon-tank 的直接测试和类型检查；不得用根目录错误 cwd 的结果替代子工程结果。

## 7. 证据索引

```text
- 全局规则：/Users/tristanzh/agent/AGENTS.md
- Agent11 云服务交接：/Users/tristanzh/agent/agent11-fishtank-monitor/HANDOVER_CLOUD_TO_AGENT12_20260725.md
- Agent11 软件交接：/Users/tristanzh/agent/agent11-fishtank-monitor/HANDOVER_SOFTWARE_CLOUD_STATUS_20260724.md
- Agent12 热状态交接：/Users/tristanzh/agent/agent12-fishtank-3Dtwin/docs/operations/HANDOVER_AGENT12_THERMAL_STATE_20260725.md
- 用户提供的真实 COS v2 快照：当前对话消息，未写入任何凭据
- 工作流对齐线程：3D数字孪生鱼缸v2（已发送任务对齐消息）
```
