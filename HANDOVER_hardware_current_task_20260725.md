### ☀️ 次日启动胶囊 (Boot Prompt)

请在明天开启新对话时，直接复制以下指令发给系统：

```text
请静默读取并完全理解当前目录下的 `HANDOVER_hardware_current_task_20260725.md`。
*注：“静默读取”指不要逐段复述全文，只需精准提炼核心卡点、下一步行动和需要 TZ 确认的事项。*

1. 请将本对话的逻辑分支锁定为：【温控检测-硬件当前任务】，并在你回复的第一句话使用 Markdown 的 H1 标题 (`# 温控检测-硬件当前任务 工作流重启`) 输出，以便系统自动重命名此对话。
2. 请在执行任何操作前，简要复述当前的【核心卡点】与【下一步行动】。
3. 请在回复末尾明确提示：『请 TZ 确认：是按计划执行，还是需要进行微调？』
4. 在收到 TZ 明确授权前，不得执行文件修改、命令运行、方案推进或任务范围扩展；仅允许复述理解、指出不确定项并等待确认。
```

---

## 1. 第一性原理与项目上下文

### 当前项目根

```text
/Users/tristanzh/agent/agent11-fishtank-monitor
```

### 当前工作流主题

```text
温控检测-硬件当前任务
```

### 核心目的

第一缸温控硬件工作流的本质，是把真实 ESP32-S3 + 双 DS18B20 低压监控装置，从桌面验证推进到真实采样、真实心跳、云服务可消费状态，再进入防水盒、走线和下缸前安装阶段。任何状态都必须来自真实硬件或已提交/已验证的项目证据，不能用模拟心跳、伪造 COS 状态或旧值冒充当前读数。

### 当前上下文边界

本 handover 只覆盖当前温控检测硬件任务及其与云服务、软件、Agent12 的交接边界。它不覆盖真实密钥、token、SCF 控制台操作细节、Agent12 3D 实现细节、Git 提交流程，也不授权任何 220V 控制。

---

## 2. 今日完成事项

### 修改文件清单

```text
- HANDOVER_hardware_current_task_20260725.md
  - 修改内容：新增当前硬件任务收工交接文档。
  - 证据来源：本次用户明确要求“将当前任务相关信息做收工交接文档”，并按 handover skill 生成。
```

当前仓库状态在生成本文件前显示：

```text
?? HANDOVER_cloud_state_projection_20260725_v2.md
```

`git diff --stat` 和 `git diff --name-only` 在生成本文件前无已跟踪未提交变更；今日核心代码和文档变更已由本地 commit 记录承载。

今日本地 commit 证据：

```text
de475b2 chore(agent11-fishtank-monitor): commit selected repo changes
  A HANDOVER_hardware_realtime_20260725.md

f181c5d chore(agent11-fishtank-monitor): commit selected repo changes
  A HANDOVER_CLOUD_SHIFT_20260725.md
  A HANDOVER_CLOUD_TO_AGENT12_20260725.md
  A HANDOVER_SOFTWARE_CLOUD_STATUS_20260724.md
  A HANDOVER_thermal_twin_alignment_20260725.md
  M cloud/tencent-scf/*
  A cloud/tencent-scf/src/state_read_handler.mjs
  A cloud/tencent-scf/test/state_read_handler.test.mjs
  A docs/superpowers/plans/2026-07-25-heartbeat-state-snapshot.md
  A docs/superpowers/specs/2026-07-25-heartbeat-state-snapshot-design.md
  M include/config.hpp
  A lib/active_event_snapshot/*
  M lib/heartbeat_contract/*
  M platformio.ini
  M src/heartbeat_notifier.*
  M src/main.cpp
  A test/test_active_event_snapshot/*
  M test/test_heartbeat_contract/*
  M test/test_temperature_engine/test_config.cpp
  M test/verify_docs.sh
```

### 实现逻辑

本硬件线程亲自确认过的内容：

```text
- ESP32-S3 串口存在并可读：/dev/cu.usbmodem5B910349491。
- 真实 ESP32-S3 正在运行固件。
- 双 DS18B20 采样持续有效，串口字段为 main_tank / sump_tank。
- 已看到真实硬件侧 `heartbeat delivered`。
- 没有发送模拟心跳。
- 没有伪造 COS 状态。
- 没有输出 token、设备密钥、Function URL、HMAC 签名或 CA PEM。
- 已通知温控检测-云服务、温控检测-软件、Agent12 工作流继续执行；非必须用户确认时，应执行完成后再通知 TZ。
```

已提交交接文件中的其他工作流状态：

```text
- HANDOVER_CLOUD_TO_AGENT12_20260725.md 表明云服务端开发阶段已完成并进入验收测试阶段。
- 该文件记录 Agent12 读取接口、错误契约、字段映射和安全边界。
- 该文件还记录“真实高温事件已通过 ESP32 -> SCF -> Bark 手机链路验证”等云端验收信息。
```

注意：上一段属于已提交交接文件证据，不等同于本硬件线程重新独立复验了每一个云端场景。

---

## 3. 已作出的关键决策

```text
- 决策：硬件侧以真实串口 `heartbeat delivered` 作为云服务和 Agent12 继续复核的触发证据。
- 原因：云服务此前只读诊断显示 503 state_unavailable 的阻塞点在“没有真实 tank01 状态对象”；真实心跳成功后，云服务应复核 COS 和 GET 200。
- 放弃的替代方案：不模拟心跳、不手工写 COS、不伪造 Agent12 状态。
- 证据来源：用户委托、串口日志、已发送到相关工作流的消息。
```

```text
- 决策：相关工作流后续自动执行，非必要不再打断 TZ。
- 原因：TZ 明确要求“如果非必须我确认，就将任务执行完成之后再通知我”。
- 放弃的替代方案：不在每个只读复核步骤都请求 TZ 确认。
- 证据来源：用户明确指令。
```

```text
- 决策：本次收工必须按标准 handover skill 执行。
- 原因：TZ 指出之前没有按 skill 定义的 handover 收工；AGENTS.md Required End Routine 也要求遇到收工/交接立即执行 handover skill。
- 放弃的替代方案：不再用普通聊天式“今天收工”回复替代交接文档。
- 证据来源：用户指出问题、AGENTS.md、/Users/tristanzh/agent/.ai_skills/handover/SKILL.md。
```

---

## 4. 验证状态

### 已运行命令

```bash
ls /dev/cu.*
/Users/tristanzh/Library/Python/3.9/bin/pio device monitor -p /dev/cu.usbmodem5B910349491 -b 115200
date '+%Y-%m-%d %H:%M:%S %Z'
git status --short
git diff --stat
git diff --name-only
git diff --cached --name-only
git log --since=midnight --name-status --oneline
```

### 验证结果

```text
- 串口存在：通过。
- 双探头采样：通过。
- 真实设备心跳：通过，已看到 `heartbeat delivered`。
- 敏感信息保护：通过，本硬件线程未输出 token 或密钥。
- 云端 COS/GET/Agent12 最终恢复：本硬件线程未直接验证，已通知对应工作流执行。
- 当前 handover 文件生成前的 git 工作区：仅发现未跟踪 `HANDOVER_cloud_state_projection_20260725_v2.md`。
```

### 串口证据

```text
观察时间: 2026-07-25 20:26:40 CST
串口: /dev/cu.usbmodem5B910349491
设备状态: 真实 ESP32-S3 正在运行
双探头采样: 有效、持续
心跳上报: 已看到真实 heartbeat delivered
```

```text
sample at_ms=14954424 main_tank=19.31C sump_tank=24.00C
sample at_ms=14984454 main_tank=19.37C sump_tank=24.25C
sample at_ms=15014483 main_tank=19.37C sump_tank=24.50C
sample at_ms=15044512 main_tank=19.44C sump_tank=24.69C
heartbeat delivered
sample at_ms=15074536 main_tank=19.44C sump_tank=24.81C
```

### 未验证项与手动验证路径

```text
- 未验证项：COS 是否已稳定存在 devices/tank01/state.json。
  - 原因：本硬件线程不直接操作 COS。
  - 手动路径：由温控检测-云服务只读复核。

- 未验证项：Agent11 GET 是否稳定返回 200 FishTankStateV1。
  - 原因：已转交云服务/Agent12 工作流。
  - 手动路径：使用受控只读 token 调用 GET，不输出 token。

- 未验证项：Agent12 upstream 是否恢复 healthy。
  - 原因：这是 Agent12 工作流职责。
  - 手动路径：读取 Agent12 状态接口和 3D 联动结果，不伪造状态。

- 未验证项：防水盒、PG7/M12、USB-C 面板线、走线、下缸与 48-72 小时观察。
  - 原因：今天硬件任务尚未进入正式安装。
  - 手动路径：下一次硬件工作流从盒体布局和开孔前标点开始。
```

---

## 5. 未解决的风险/报错

```text
- 风险/报错：云服务/Agent12 的最终恢复状态需看对应工作流回报。
  - 当前状态：硬件侧已提供真实 heartbeat delivered；相关工作流已被通知继续自动执行。
  - 影响范围：如果 COS/GET/Agent12 未恢复，需要继续定位云端或 Agent12 层，不应回退到伪造数据。
  - 建议下一步：先查看三个相关工作流的完成回报。
```

```text
- 风险/报错：当前硬件仍非最终安装形态。
  - 当前状态：真实采样和真实心跳已通过；防水盒与下缸安装未完成。
  - 影响范围：不能宣称长期无人值守、正式上线或可在潮湿环境稳定运行。
  - 建议下一步：继续防水盒布局、走线固定、应力释放、滴水弯和下缸前检查。
```

```text
- 风险/报错：工作区存在未跟踪文件 HANDOVER_cloud_state_projection_20260725_v2.md。
  - 当前状态：该文件不是本硬件 handover 生成目标。
  - 影响范围：后续 Git/交接时需要判断是否属于云服务或 Agent12 工作流。
  - 建议下一步：由对应工作流确认保留、提交或删除；本硬件工作流不擅自处理。
```

---

## 6. 下一步行动 (原子化设计)

- [ ] **Action-1 (查看协作回报)**：检查 `温控检测-云服务`、`温控检测-软件`、`3D数字孪生鱼缸 / Agent12` 是否已回报 COS/GET/upstream 验证结果。
- [ ] **Action-2 (成功路径)**：若 Agent11 GET 已返回 `200 FishTankStateV1` 且 Agent12 upstream 恢复，记录验收证据并进入硬件安装准备。
- [ ] **Action-3 (失败路径)**：若仍未恢复，要求对应工作流只读定位具体层级，不在硬件侧伪造状态。
- [ ] **Action-4 (硬件安装准备)**：打开防水盒、PG7/M12、防水 USB-C 面板线、扎带座、两支已标记探头，先做桌面布局，不钻孔。
- [ ] **Action-5 (低压复验)**：正式装盒前重新执行短路检查、3.3V 电压检查、双探头串口采样检查。
- [ ] **Action-6 (下缸前门槛)**：只有盒内固定、应力释放、滴水弯、低压复验都通过后，才进入探头下缸与 48-72 小时只监控观察。

---

## 7. 证据索引

```text
- AGENTS.md：
  - /Users/tristanzh/agent/AGENTS.md
  - Required End Routine 指向 /Users/tristanzh/agent/.ai_skills/handover/SKILL.md。

- handover skill：
  - /Users/tristanzh/agent/.ai_skills/handover/SKILL.md
  - 触发词包含“收工”“今日收工”“交接”“handover”。

- git status / diff / log：
  - `git status --short`：生成本文件前仅 `?? HANDOVER_cloud_state_projection_20260725_v2.md`。
  - `git diff --stat`：生成本文件前无输出。
  - `git diff --name-only`：生成本文件前无输出。
  - `git log --since=midnight --name-status --oneline`：包含 `de475b2` 和 `f181c5d` 两个今日 commit。

- 用户确认：
  - TZ 要求通知对应工作流继续执行，非必须确认则完成后再通知。
  - TZ 指出应按 handover skill 执行收工。
  - TZ 要求回归当前任务并生成收工交接文档。

- 对话上下文记录：
  - 硬件串口真实 `heartbeat delivered` 已捕获。
  - 相关工作流已被直接通知继续验证 COS、GET 200 和 Agent12 upstream。
```

