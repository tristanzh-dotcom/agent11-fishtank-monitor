### ☀️ 次日启动胶囊 (Boot Prompt)

请在明天开启新对话时，直接复制以下指令发给系统：

```text
请静默读取并完全理解当前目录下的 `HANDOVER_multi_tank_bark_temperature_monitor_20260909.md`。
*注：“静默读取”指不要逐段复述全文，只需精准提炼核心卡点、下一步行动和需要 TZ 确认的事项。*

1. 请将本对话的逻辑分支锁定为：【四缸五探头本地 Bark 监测接入前收尾】，并在你回复的第一句话使用 Markdown 的 H1 标题 (`# 四缸五探头本地 Bark 监测接入前收尾 工作流重启`) 输出，以便系统自动重命名此对话。
2. 请在执行任何操作前，简要复述当前的【核心卡点】与【下一步行动】。
3. 仅当本次交接确实存在需要 TZ 裁决的未决事项时，才在回复中用简洁自然语言说明该事项；不得把固定确认句追加到普通对话或无待决事项的回复末尾。
4. 在收到 TZ 明确授权前，仅允许以只读方式读取指定 handover，以及确定其权威项目根所必需的 `AGENTS.md`；可据此提炼核心卡点、下一步行动和待确认事项。不得修改文件、运行测试/构建/服务、调用外部系统、操作硬件或执行 Git 写入，也不得推进方案或扩大任务范围。
```

## 1. 第一性原理与项目上下文

### 当前项目根

```text
/Users/tristanzh/agent/agent11-fishtank-monitor
```

### 当前工作流主题

```text
四缸五探头本地 Bark 监测接入前收尾
```

### 核心目的

在现有包包缸主缸和回水缸两个 DS18B20 基础上，为老四缸、小黑缸、毛毛缸增加固定 ROM 的独立本地温度监测和 Bark 告警，并每天上海时间 09:00、18:00 发送五路温度汇总。

### 当前上下文边界

软件、主机测试、ESP32-S3 目标板构建和接入前文档已完成。三个新增实物 ROM、上传、真实 Bark、NTP、现场校准、总线稳定性、安装和 48–72 小时观察尚未执行。三个小缸仍不进入 Tencent SCF、FishTankStateV1、Agent12 或 Web；系统不控制任何鱼缸执行器或市电设备。

## 2. 今日完成事项

### 修改文件清单

今日 Git 证据包含本地提交 `518a6d0`（`chore(agent11-fishtank-monitor): commit selected repo changes`）以及提交后的未提交工作区改动。

当前未提交的核心文件：

- `README.md`
  - 固定 PlatformIO Core/平台使用说明，上传命令要求显式 `--upload-port`。
  - 修正 transport 测试命令并说明五探头接入清单。
  - 证据来源：`git diff`。
- `platformio.ini`
  - 将 Espressif 平台固定到 pioarduino `55.03.36` 发布包。
  - 证据来源：`git diff`、目标板构建输出。
- `src/main.cpp`
  - 小缸队列和日报路径服从 `runtime_config.bark_enabled`。
  - 证据来源：`git diff`、目标板构建。
- `lib/transport_contract/src/transport_contract.cpp`
  - 主缸消息带“包包缸”；探头故障 resolved 显示有效恢复温度，opened/reminder 显示“无有效读数”。
  - 证据来源：`git diff`、transport contract 测试。
- `test/test_transport_contract/test_transport_contract.cpp`
  - 增加主缸名称、故障三状态和恢复温度断言。
  - 证据来源：`git diff`、transport contract 测试。
- `docs/five-probe-commissioning.md`
  - 修正备用探头数量说明并提供五探头 ROM、校准、干态和现场观察清单。
  - 证据来源：`git diff`。
- `docs/superpowers/plans/2026-09-09-multi-tank-bark-temperature-monitor.md`
  - 记录“包包缸”文案决定已取代旧的主消息逐字节断言要求。
  - 证据来源：`git diff`。
- `docs/pre-hardware-closeout-audit.md`
  - 新增四项缺口修复和收尾审计记录；当前为未跟踪文件。
  - 证据来源：工作区状态、文件内容。

`518a6d0` 已包含本期主要多缸实现、配置、读取器、Bark 合同、日报、队列、主机测试、硬件安装补充和设计/计划文件；当前工作区仍有上述未提交收尾改动，未提交或推送 Git。

### 实现逻辑

- 五个逻辑角色为 `main_tank_sensor`、`sump_return_sensor`、`laosi_tank`、`xiaohei_tank`、`maomao_tank`。主缸和回水缸使用已有固定 ROM；三个新增 ROM 当前仍为全零未配置。
- 新增小缸使用独立 `TemperatureEngine`，共享一个容量 16 的带鱼缸身份队列，只通过本地 Bark 投递。
- 未配置或重复 ROM 不按 OneWire 发现顺序回退；状态分别输出 `unconfigured`、`duplicate`，有效绑定输出 `valid`。
- 每次采样串口输出主缸、回水缸和三个小缸；小缸同时打印绑定状态。
- 老四缸低温阈值为 N2 `<22.5°C`、N3 `<20.5°C`、恢复 `>=23.5°C`；小黑缸和毛毛缸为 N2 `<23.5°C`、N3 `<22.5°C`、恢复 `>=24.5°C`。小缸高温阈值为 `>27.5°C`/`>28.5°C`，持续和恢复时间沿用既有策略。
- 每日汇总只在上海时间 `[09:00:00,09:01:00)`、`[18:00:00,18:01:00)` 生成，保留固定采样快照，失败最多保留 10 分钟；不做跨重启持久化和错过窗口补发。
- `bark_enabled=false` 时，小缸事件不入队、日报不生成且不发送；温度状态机和现有心跳继续运行。
- 主缸和小缸 sensor fault 的 opened/reminder 显示“无有效读数”，resolved 使用事件中的有效温度；主缸标题和正文包含“包包缸”。

## 3. 已作出的关键决策

- 决策：三个小缸只做本地 Bark 监测，不扩展 Web、Agent12、FishTankStateV1 或 Tencent SCF。
  - 原因：用户明确不需要 Web Agent 页面，保持公共两路状态合同稳定。
  - 放弃的替代方案：云端上传三路小缸并由 SCF 定时发送日报。
  - 证据来源：已批准设计、用户授权实现。
- 决策：老四缸低温恢复阈值为 `23.5°C`。
  - 原因：用户明确确认。
  - 放弃的替代方案：使用其他小缸的 `24.5°C`。
  - 证据来源：用户明确确认与配置测试。
- 决策：新增 ROM 必须现场识别后固定绑定，禁止发现顺序回退。
  - 原因：避免探头交换造成错误鱼缸告警。
  - 放弃的替代方案：按 OneWire discovery index 自动分配角色。
  - 证据来源：设计方案、配置实现和接入清单。
- 决策：PlatformIO 平台固定为 pioarduino `55.03.36`，本机已验证 Core `6.2.0` / Python `3.12`。
  - 原因：解决平台升级后旧 Arduino framework 缓存冲突，并使默认构建入口可重复使用。
  - 放弃的替代方案：继续使用浮动 `espressif32` 或 Python 3.9 旧入口。
  - 证据来源：`platformio.ini`、PlatformIO 构建输出、`pip check`。

## 4. 验证状态

### 已运行命令

主机合同与文档验证：

```bash
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_simulation.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_simulation_tests && ./.build/temperature_simulation_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include test/test_temperature_engine/test_config.cpp -o .build/config_tests && ./.build/config_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/scoped_event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && ./.build/transport_contract_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/active_event_snapshot/include -Ilib/heartbeat_contract/include test/test_heartbeat_contract/test_heartbeat_contract.cpp lib/active_event_snapshot/src/active_event_snapshot.cpp lib/heartbeat_contract/src/heartbeat_contract.cpp -o .build/heartbeat_contract_tests && ./.build/heartbeat_contract_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_daily_summary/test_daily_summary.cpp lib/transport_contract/src/daily_summary.cpp lib/transport_contract/src/transport_contract.cpp -o .build/daily_summary_tests && ./.build/daily_summary_tests
sh test/verify_docs.sh
git diff --check
/Users/tristanzh/.platformio/penv/bin/python -m pip check
```

目标板构建：

```bash
/Users/tristanzh/.platformio/penv/bin/pio run -e waveshare_esp32s3_n16r8
```

### 验证结果

- 温度引擎、仿真、配置、transport contract、heartbeat contract、daily summary：全部通过。
- `sh test/verify_docs.sh`：`documentation checks passed`。
- `git diff --check`：通过。
- `pip check`：`No broken requirements found.`。
- PlatformIO：`SUCCESS`；平台 `55.3.36`、Arduino `3.3.6`；RAM `53520` bytes，Flash `1318315` bytes。
- PlatformIO 构建保留 OneWire 2.3.8 的两条既有 `#undef` 额外 token 警告；未阻止构建。

### 未验证项与手动验证路径

- 未上传固件。硬件阶段需先用 `pio device list` 确认目标串口，再使用 README 中带 `--upload-port` 的命令；不能仅凭 host/build 结果推断烧录成功。
- 未识别三个新增实物 ROM。断电接线、逐支扫描并用断开法确认角色，记录到 [五探头接入清单](docs/five-probe-commissioning.md) 后再修改 `include/config.hpp`。
- 未验证五探头长线 OneWire 稳定性。先桌面 15 分钟同水校准，再至少 120 分钟干态运行，然后安装并观察 48–72 小时。
- 未调用真实 Bark、Wi-Fi、NTP 或手机收信。现场需核对告警、09:00/18:00 汇总、上海时间和失败重试行为。
- 未运行 Tencent SCF 或 cloud/bark-forwarder 全套测试；本次没有云端代码或公共状态合同改动，按项目治理未触发 Level 4 全云端回归。

## 5. 未解决的风险/报错

- 风险/报错：OneWire 2.3.8 编译出现两条既有 `extra tokens at end of #undef directive` 警告。
  - 当前状态：固件仍成功链接和生成 `firmware.bin`。
  - 影响范围：第三方 OneWire 编译告警；当前未见新增代码错误证据。
  - 建议下一步：保持记录，除非现场出现 OneWire 读数异常，否则不要为此扩展范围。
- 风险/报错：PlatformIO 构建识别的是通用 `esp32-s3-devkitc-1` / `N8` 描述，目标配置另设 16MB 和 `BOARD_HAS_PSRAM`。
  - 当前状态：目标板构建通过，但这只是构建配置证据，不是实物板型识别证据。
  - 影响范围：首次上传前需确认实际 ESP32-S3-DEV-KIT-N16R8-M 与当前 board/flash/PSRAM 设置相容。
  - 建议下一步：硬件阶段在上传前核对板上型号、串口和 PlatformIO upload 参数；不要把通用 board 构建结果当作实物验收。
- 风险/报错：平台 URL 已固定，但 OneWire、DallasTemperature、PubSubClient 仍由 `platformio.ini` 的版本范围解析，完全离线或逐字节复现尚未证明。
  - 当前状态：当前本机依赖已成功安装并构建。
  - 影响范围：换机器或清空缓存时可能重新解析依赖。
  - 建议下一步：硬件接入前保持当前缓存和已验证环境；只有出现跨机器复现要求时再单独制定锁定方案。

## 6. 下一步行动 (原子化设计)

- [ ] **Action-1 (只读恢复)**：新对话先读取本文件和项目 `AGENTS.md`，确认工作区与硬件阶段边界。
- [ ] **Action-2 (物料核对)**：打开 `docs/five-probe-commissioning.md`，现场清点五支探头、线缆、接头和一个总线 4.7kΩ 上拉。
- [ ] **Action-3 (桌面接入)**：断开 USB 后完成五探头低压接线；上电运行当前固件，记录五个 ROM 和每路串口读数。
- [ ] **Action-4 (角色确认)**：逐支断开新增探头，确认老四缸、小黑缸、毛毛缸与 ROM 的对应关系；把结果记录在清单。
- [ ] **Action-5 (配置更新)**：仅将人工确认的三个 ROM 写入 `include/config.hpp`，运行配置测试和 PlatformIO 构建。
- [ ] **Action-6 (上传门槛)**：确认实际设备和端口后，使用显式 `--upload-port` 上传；记录串口启动、五路读数和 `binding=valid`。
- [ ] **Action-7 (真实链路)**：验证 Wi-Fi/NTP、Bark 告警、故障恢复、09:00/18:00 汇总与手机收信。
- [ ] **Action-8 (观察记录)**：先完成 15 分钟同水校准和 120 分钟桌面稳定性，再进入 48–72 小时只监控观察。

## 7. 证据索引

```text
- AGENTS.md：/Users/tristanzh/agent/agent11-fishtank-monitor/AGENTS.md；定义本项目软件、云端、上传、安装、校准、观察的分阶段证据边界。
- Git 状态：`git status --short`；当前存在未提交的代码、文档、测试和本 handover 文件。
- Git 差异：`git diff --stat` / `git diff --name-only` / `git diff --check`；收尾改动集中在文案、Bark 开关、平台固定、接入文档和 transport 测试。
- Git 历史：`git log --since='midnight' --name-status --oneline --decorate`；今日 HEAD 为 `518a6d0`，包含此前多缸实现主线。
- 用户确认：老四缸低温恢复使用 `23.5°C`；接受设计审计建议并授权完成硬件接入前开发和测试；不需要 Web Agent 页面。
- 方案文件：`docs/superpowers/specs/2026-09-09-multi-tank-bark-temperature-monitor-design.md`。
- 实现计划：`docs/superpowers/plans/2026-09-09-multi-tank-bark-temperature-monitor.md`。
- 收尾审计：`docs/pre-hardware-closeout-audit.md`。
- 构建证据：PlatformIO Core `6.2.0`、Python `3.12`、pioarduino 平台 `55.03.36`，目标构建 `SUCCESS`。
```
