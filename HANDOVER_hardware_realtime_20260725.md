### ☀️ 次日启动胶囊 (Boot Prompt)

请在明天开启新对话时，直接复制以下指令发给系统：

```text
请静默读取并完全理解当前目录下的 `HANDOVER_hardware_realtime_20260725.md`。
*注：“静默读取”指不要逐段复述全文，只需精准提炼核心卡点、下一步行动和需要 TZ 确认的事项。*

1. 请将本对话的逻辑分支锁定为：【温控检测-硬件实时联调】，并在你回复的第一句话使用 Markdown 的 H1 标题 (`# 温控检测-硬件实时联调 工作流重启`) 输出，以便系统自动重命名此对话。
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
温控检测-硬件实时联调
```

### 核心目的

第一缸温控 MVP 的硬件侧目标，是让真实 ESP32-S3 + 双 DS18B20 在低压、只监控、不控制市电的边界内，持续产生可信温度采样和真实设备心跳，为云服务、软件和 Agent12 提供不可伪造的真实状态输入。

### 当前上下文边界

本 handover 覆盖 2026-07-25 本硬件工作流实际确认的硬件/串口/心跳证据，以及已通知相关工作流的协作状态。不覆盖云服务端代码实现细节、Agent12 UI/3D 展示实现、Git 提交、真实密钥内容或任何 220V 控制。

---

## 2. 今日完成事项

### 修改文件清单

```text
- HANDOVER_hardware_realtime_20260725.md
  - 修改内容：新增本次硬件实时联调收工交接文档。
  - 证据来源：本文件由 handover skill 补执行生成。
```

本工作流今日没有主动提交 Git，也没有执行 Git 写操作。当前工作区存在大量已修改/未跟踪文件，其中包括云服务、软件、Agent12 联调相关改动；这些不全部归属本硬件工作流，次日接手时必须按各自交接文件和 `git diff` 重新确认来源。

当前 `git status --short` 显示的主要未提交变更包括：

```text
M cloud/tencent-scf/README.md
M cloud/tencent-scf/scripts/send-signed-heartbeat.mjs
M cloud/tencent-scf/src/bark_notifier.mjs
M cloud/tencent-scf/src/heartbeat_contract.mjs
M cloud/tencent-scf/src/heartbeat_handler.mjs
M cloud/tencent-scf/src/runtime_config.mjs
M cloud/tencent-scf/src/scf_runtime.mjs
M cloud/tencent-scf/test/*.mjs
M include/config.hpp
M lib/heartbeat_contract/include/heartbeat_contract.hpp
M lib/heartbeat_contract/src/heartbeat_contract.cpp
M platformio.ini
M src/heartbeat_notifier.cpp
M src/heartbeat_notifier.hpp
M src/main.cpp
M test/test_heartbeat_contract/test_heartbeat_contract.cpp
M test/test_temperature_engine/test_config.cpp
M test/verify_docs.sh
?? HANDOVER_CLOUD_TO_AGENT12_20260725.md
?? HANDOVER_SOFTWARE_CLOUD_STATUS_20260724.md
?? HANDOVER_thermal_twin_alignment_20260725.md
?? cloud/tencent-scf/src/state_read_handler.mjs
?? cloud/tencent-scf/test/state_read_handler.test.mjs
?? docs/superpowers/plans/2026-07-25-heartbeat-state-snapshot.md
?? docs/superpowers/specs/2026-07-25-heartbeat-state-snapshot-design.md
?? lib/active_event_snapshot/
?? test/test_active_event_snapshot/
```

### 实现逻辑

```text
- 真实 ESP32-S3 已通过串口确认正在运行。
- 双 DS18B20 持续采样有效。
- 串口输出字段为 main_tank / sump_tank。
- 已看到真实硬件侧 `heartbeat delivered`。
- 已将硬件侧安全证据通知给温控检测-云服务、温控检测-软件、Agent12 工作流。
- 已明确要求相关工作流：非必须 TZ 提供权限、密钥、硬件动作或外部控制台操作时，直接执行完成后再通知 TZ。
```

---

## 3. 已作出的关键决策

```text
- 决策：硬件侧以真实串口 `heartbeat delivered` 作为云服务复核 COS/GET/Agent12 的交接门槛。
- 原因：云服务侧只读诊断已证明 503 state_unavailable 的剩余阻塞是没有真实 tank01 状态对象；不能用模拟心跳或伪造 COS 状态绕过。
- 放弃的替代方案：不发送模拟心跳，不手工写 COS，不向 Agent12 注入假状态。
- 证据来源：用户委托、硬件串口输出、已发送到相关工作流的通知。
```

```text
- 决策：硬件工作流今日只回传安全证据，不输出 token、Function URL、设备共享密钥、HMAC 签名或 CA PEM。
- 原因：这是硬件/云联调的安全边界，且用户明确要求不要发送 token。
- 放弃的替代方案：不在聊天、日志或交接文件中记录任何敏感值。
- 证据来源：用户明确约束与项目安全边界。
```

---

## 4. 验证状态

### 已运行命令

```bash
ls /dev/cu.*
/Users/tristanzh/Library/Python/3.9/bin/pio device monitor -p /dev/cu.usbmodem5B910349491 -b 115200
date '+%Y-%m-%d %H:%M:%S %Z'
```

### 验证结果

```text
- 串口存在：通过
  - /dev/cu.usbmodem5B910349491 存在，作为 USB Single Serial 串口使用。

- 双探头采样：通过
  - 已看到连续 sample 输出。

- 真实设备心跳：通过
  - 已看到 `heartbeat delivered`。

- 敏感信息保护：通过
  - 本工作流未输出 token、真实密钥、HMAC 签名或 CA PEM。

- COS/GET/Agent12 恢复：未由本硬件工作流验证
  - 已通知相关工作流继续复核。
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
- 未验证项：COS 是否已经出现 `devices/tank01/state.json`
  - 原因：这是云服务工作流任务，本硬件工作流未访问 COS。
  - 手动路径：由温控检测-云服务只读复核 COS 对象和 Agent11 GET。

- 未验证项：Agent11 GET 是否从 503 恢复到 200 FishTankStateV1
  - 原因：已转交云服务/Agent12 工作流。
  - 手动路径：使用受控只读 token 调用 GET，不输出 token。

- 未验证项：Agent12 upstream 是否恢复
  - 原因：这是 Agent12 工作流任务。
  - 手动路径：由 Agent12 读取 `/api/agent12/status` 和下游状态，不伪造数据。

- 未验证项：防水盒安装、下缸、48-72 小时观察
  - 原因：今天收工前未继续物理安装。
  - 手动路径：下次硬件工作流从防水盒布局和走线规划开始。
```

---

## 5. 未解决的风险/报错

```text
- 风险/报错：云服务侧此前只读 GET 为 503 state_unavailable。
  - 当前状态：硬件侧已看到真实 heartbeat delivered；云服务尚需复核 COS 对象和 GET 200。
  - 影响范围：Agent12 upstream 可能仍 degraded，直到 Agent11 可读状态恢复。
  - 建议下一步：等待/读取云服务与 Agent12 工作流回报；如仍失败，按层级定位：设备 delivered 但 COS 未出现、COS 已出现但 GET 非 200、GET 已 200 但 Agent12 未恢复。
```

```text
- 风险/报错：当前硬件不是最终装盒/下缸形态。
  - 当前状态：桌面硬件和真实心跳已通过；防水盒、PG7/M12、USB-C 面板线、应力释放、滴水弯尚未安装验收。
  - 影响范围：不能宣称长期无人值守或正式上线。
  - 建议下一步：下一次硬件工作流从盒体布局、开孔前标点、走线固定开始。
```

```text
- 风险/报错：串口温度值有明显变化。
  - 当前状态：这是桌面/手动测试环境，不能代表鱼缸稳定水温。
  - 影响范围：云端状态可用于链路验证，但不能用于鱼缸真实温控结论。
  - 建议下一步：正式下缸后再做 48-72 小时只监控观察。
```

---

## 6. 下一步行动 (原子化设计)

- [ ] **Action-1 (查看相关工作流回报)**：打开 `温控检测-云服务`、`温控检测-软件`、`3D数字孪生鱼缸 / Agent12`，确认它们是否已完成 COS/GET/upstream 复核。
- [ ] **Action-2 (云服务成功路径)**：若云服务回报 Agent11 GET 已恢复 `200 FishTankStateV1`，转入 Agent12 真实状态联动验收。
- [ ] **Action-3 (云服务失败路径)**：若仍是 503 或 upstream degraded，要求对应工作流只读定位具体断点，不在硬件侧伪造状态。
- [ ] **Action-4 (硬件安装准备)**：硬件继续时，先不下缸，打开防水盒、PG7/M12、USB-C 面板线、扎带座和两支已标记探头，做盒内布局。
- [ ] **Action-5 (正式安装验证)**：完成盒内布局后，重新做低压短路检查、3.3V 电压检查、双探头串口采样检查，再进入下缸前固定。

---

## 7. 证据索引

```text
- AGENTS.md：
  - /Users/tristanzh/agent/AGENTS.md
  - 其中 Required End Routine 明确要求：用户请求 handoff/closing/end-of-day/今日收工 时，立即读取并执行 `/Users/tristanzh/agent/.ai_skills/handover_skill.md`。

- handover skill：
  - /Users/tristanzh/agent/.ai_skills/handover_skill.md
  - 触发词包含“收工”“今日收工”“handover”。

- git status / diff / log：
  - `git status --short` 显示大量未提交变更，涉及硬件、软件、云服务和 Agent12 交接文件。
  - `git diff --stat` 显示 23 个已跟踪文件变更，660 insertions / 89 deletions。
  - `git log --since=midnight --name-status --oneline` 今日无本地 commit 输出。

- 用户确认：
  - 用户要求硬件侧确认真实 ESP32-S3 已运行并看到 `heartbeat delivered`。
  - 用户要求通知对应工作流继续执行，非必须确认则完成后再通知。
  - 用户指出“今天收工”应按 handover skill 执行。

- 对话上下文记录：
  - 已通知 `温控检测-云服务`、`温控检测-软件`、`3D数字孪生鱼缸 / Agent12`。
  - 已回传硬件侧安全证据：真实串口、双探头采样、`heartbeat delivered`。
```

