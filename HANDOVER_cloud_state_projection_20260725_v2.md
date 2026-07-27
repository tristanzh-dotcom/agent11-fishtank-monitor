### ☀️ 次日启动胶囊 (Boot Prompt)

请在明天开启新对话时，直接复制以下指令发给系统：

```text
请静默读取并完全理解当前目录下的 `HANDOVER_cloud_state_projection_20260725_v2.md`。
*注：“静默读取”指不要逐段复述全文，只需精准提炼核心卡点、下一步行动和需要 TZ 确认的事项。*

1. 请将本对话的逻辑分支锁定为：【Agent11 云状态投影与 Agent12 联调】，并在你回复的第一句话使用 Markdown 的 H1 标题 (`# Agent11 云状态投影与 Agent12 联调 工作流重启`) 输出，以便系统自动重命名此对话。
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
Agent11 云状态投影与 Agent12 联调
```

### 核心目的

将真实 ESP32-S3 双探头心跳保存到私有 COS，并由 Agent11 只读接口稳定投影成 Agent12 可消费的 `FishTankStateV1`。

### 当前上下文边界

- 本次交接覆盖腾讯云 SCF/COS、Agent11 只读 GET 和 Agent12 真实轮询验收。
- 不覆盖 ESP32 固件、温度阈值、Bark key、设备 HMAC 密钥、COS 凭据、硬件接线或 Agent12 视觉逻辑。
- 不使用模拟心跳、伪造 COS 状态或前端猜测温度事件。

## 2. 今日完成事项

### 修改文件清单

以下两项是本次工作流实际新增的未提交文件；仓库中其他 dirty 文件未归因于本次工作流：

```text
- cloud/tencent-scf/src/state_read_handler.mjs
  - 修改内容：允许 `temperature_gradient` 事件通过 FishTankStateV1 投影。
  - 证据来源：当前工作流 apply_patch；部署后真实 GET 返回 200。
- cloud/tencent-scf/test/state_read_handler.test.mjs
  - 修改内容：增加 temperature_gradient 投影回归测试。
  - 证据来源：当前工作流 apply_patch；聚焦测试实际通过。
```

部署包：

```text
.build/tencent-scf-state-api.zip
```

该构建产物由构建脚本生成，不作为长期源码变更。

### 实现逻辑

- 真实状态中的 `temperature_gradient` 不再使整个状态投影失败。
- Agent11 仍只接受 GET、固定路径 `/api/v1/devices/tank01/state` 和独立只读 token。
- COS 仍为私有存储，Agent12 不直接访问 COS。

## 3. 已作出的关键决策

```text
- 决策：补齐 Agent11 投影白名单中的 temperature_gradient。
- 原因：心跳契约允许该事件，而真实 COS 状态包含该事件；缺失会导致 503 state_unavailable。
- 放弃的替代方案：不在 Agent12 前端重新判断温差，也不删除真实事件来规避 503。
- 证据来源：heartbeat_contract.mjs、state_read_handler.mjs、真实 GET 200 响应。

- 决策：云服务完成与 Agent12 下游验收分开报告。
- 原因：Agent11 GET 已恢复 200，但 Agent12 status 仍为 degraded。
- 放弃的替代方案：不把 Agent12 degraded 或 null last_success_at_ms 宣称为健康。
- 证据来源：两次本地 curl 的 Agent12 status 响应。
```

## 4. 验证状态

### 已运行命令

```bash
cd /Users/tristanzh/agent/agent11-fishtank-monitor/cloud/tencent-scf
node --test test/state_read_handler.test.mjs
npm test
cd /Users/tristanzh/agent/agent11-fishtank-monitor
cloud/tencent-scf/scripts/build-package.sh .build/tencent-scf-state-api.zip
```

### 验证结果

```text
- 聚焦 state_read_handler 测试：5/5 通过。
- 腾讯云函数全量测试：42/42 通过。
- 真实 Agent11 GET：HTTP 200，FishTankStateV1 字段有效。
- 真实状态：display_c=20.38、return_c=25.06、connectivity_status=online。
- 真实事件：low_temperature_critical、temperature_gradient。
- 用户确认第二次 SCF 部署已完成；GET 200 是部署后的运行时证据。
```

### 未验证项与手动验证路径

- Agent12 poller 尚未成功轮询：`GET /api/agent12/status` 仍为 `upstream.state=degraded`、`last_success_at_ms=null`。
- 下一班次应通过 Agent00/Agent12 既有生命周期入口执行一次允许的 stop/start 或 pollOnce，然后只读轮询状态接口。
- 只有看到 `upstream.state=healthy` 且 `last_success_at_ms` 为非空时间戳，才可宣称 Agent12 真实联调完成。
- 3D 页面公开投影、主缸/底滤缸标签和事件显示尚未取得本次收工的独立浏览器证据。

## 5. 未解决的风险/报错

```text
- 风险：Agent12 下游轮询器未刷新成功。
- 当前状态：本地 8012 和 Web 3000 status 均为 degraded，last_success_at_ms=null。
- 影响范围：云端来源已恢复，但数字孪生页面尚不能宣称已接入真实生产状态。
- 建议下一步：只执行 Agent12 生命周期/轮询动作，不修改 Agent11、ESP32 或视觉逻辑。

- 风险：仓库存在大量其他 dirty/untracked 文件。
- 当前状态：git status 已观察到，未将其归因于本次云状态投影工作流，也未回滚。
- 影响范围：后续修改必须继续只触碰本交接列出的文件。
- 建议下一步：如需提交或清理，按 Agent08 Git Control 规则另行处理。
```

## 6. 下一步行动 (原子化设计)

- [ ] **Action-1 (Agent12 生命周期)**：在 Agent00/Agent12 既有控制面执行一次明确授权的 pollOnce 或 stop/start。
- [ ] **Action-2 (状态确认)**：读取 `http://127.0.0.1:8012/api/agent12/status` 和 `http://127.0.0.1:3000/api/agent12/status`。
- [ ] **Action-3 (健康门禁)**：确认 `upstream.state=healthy` 且 `last_success_at_ms` 非空；否则记录具体错误并停止扩展。
- [ ] **Action-4 (公开投影)**：验证 3D 页面主缸/底滤缸温度和事件来自同一份真实快照。
- [ ] **Action-5 (安全复核)**：确认 token、设备密钥、Bark key、COS 凭据未进入聊天、日志、浏览器或前端。

## 7. 证据索引

```text
- 全局规则：/Users/tristanzh/agent/AGENTS.md
- 收工规范：/Users/tristanzh/agent/.ai_skills/handover_skill.md
- 路由与隐私规则：/Users/tristanzh/agent/GLOBAL_MODEL_ROUTING_RECORD.md
- Git 证据：本仓库 git status --short、git diff --stat、git diff --name-only、git log --since="midnight"
- 云端交接：/Users/tristanzh/agent/agent11-fishtank-monitor/HANDOVER_CLOUD_TO_AGENT12_20260725.md
- 本次云状态交接：/Users/tristanzh/agent/agent11-fishtank-monitor/HANDOVER_CLOUD_SHIFT_20260725.md
- 当前文件：/Users/tristanzh/agent/agent11-fishtank-monitor/HANDOVER_cloud_state_projection_20260725_v2.md
```
