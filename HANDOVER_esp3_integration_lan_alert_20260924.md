### ☀️ 次日启动胶囊 (Boot Prompt)

```text
请静默读取并完全理解 `/Users/tristanzh/agent/agent11-fishtank-monitor/HANDOVER_esp3_integration_lan_alert_20260924.md`。
*注：“静默读取”指不要逐段复述全文，只需精准提炼核心卡点、下一步行动和需要 TZ 确认的事项。*

1. 请将本对话的逻辑分支锁定为：【ESP3接入与断连告警修复】，并在你回复的第一句话使用 Markdown 的 H1 标题 (`# ESP3接入与断连告警修复 工作流重启`) 输出，以便系统自动重命名此对话。
2. 请在执行任何操作前，简要复述当前的【核心卡点】与【下一步行动】。
3. 仅当本次交接确实存在需要 TZ 裁决的未决事项时，才在回复中用简洁自然语言说明该事项；不得把固定确认句追加到普通对话或无待决事项的回复末尾。
4. 在收到 TZ 明确授权前，仅允许以只读方式读取指定 handover，以及确定其权威项目根所必需的 `AGENTS.md`；可据此提炼核心卡点、下一步行动和待确认事项。不得修改文件、运行测试/构建/服务、调用外部系统、操作硬件或执行 Git 写入，也不得推进方案或扩大任务范围。
```

---

## 1. 第一性原理与项目上下文

- 权威项目根：`/Users/tristanzh/agent/agent11-fishtank-monitor`。当前对话 CWD 为 home-platform，但本次收尾修复及固件由 Agent11/ESP1 仓库负责；仅扫描并写入本仓库。
- 工作流主题：ESP3接入与断连告警修复。
- 核心目的：南美草缸接入现有六缸温度监控后，最小修复 ESP1 接收 ESP2/ESP3 状态时的缺陷，并保留证据定位偶发断连/恢复告警。
- 当前阶段：ESP1 新固件已刷写、独立镜像校验及 100 秒运行检查通过。偶发告警是否消除仍待实际运行观察。
- 边界：今天最后一次修复只改 ESP1；未修改或刷写 ESP2、ESP3、Tab5，未改温度阈值、75 秒判定、广播协议、密钥及 Wi-Fi 省电设置。
- 收工动作仅生成此文件；无新增测试、构建、硬件操作、通知发送、提交或推送。

## 2. 今日完成事项

### 用户确认的整体接入状态

以下来自本对话用户明确确认，不是本次收工重新实测：

- ESP32-C3-Zero 简称 ESP3，GPIO4 所接探头对应“南美草缸”。温度阈值沿用包包主缸。
- 用户已验收 Tab5“鱼缸”显示六缸温度，快捷查询也返回六缸，包含南美草缸。此验收发生在本次 LAN 接收修复刷写之前。
- 用户报告 ESP2、ESP3 出现 Bark 断连后恢复；同一时期 Tab5、快捷查询温度正常。供电正常为用户现场判断。
- Tab5 接收 ESP3 的独立局域网状态；快捷查询读取 ESP1 汇总上传的温度。收工未跨仓库检查其他设备固件版本，不补写未知版本。

### Git 状态及今日已提交范围

- HEAD：`1837ffc7e48e3f0d116fcf448b008db3f85dee1a`。
- 今日 Git 历史中有 `1837ffc chore(agent11-fishtank-monitor): commit selected repo changes`：涉及多缸设计文档、ESP2 接收器、新增 ESP3 接收器、transport_contract 与 daily_summary、platformio.ini、局域网密钥诊断入口、main.cpp，以及摘要/ESP3/传输合同测试。
- 此提交是本轮最小修复的基线，不能把它算成本轮产生的提交。
- 当前暂存区为空。以下修复仍未提交：5 个已跟踪文件，188 行增加、13 行删除；另有未跟踪测试与发布记录。本交接新增后也为未跟踪文件。

### 核心修改文件（收工时 git diff 与文件清单核对）

- `lib/extension_lan_state/include/extension_lan_state.hpp`
- `lib/extension_lan_state/src/extension_lan_state.cpp`
  - 接收错误长度数据包后清空缓冲；每轮最多处理 4 包；新增接收/接受/拒绝计数；保留批次内间隔和跳号证据。
- `lib/grass_lan_state/include/grass_lan_state.hpp`
- `lib/grass_lan_state/src/grass_lan_state.cpp`
  - 增加仅接收端使用的时间、间隔、序号、跳号和会话变化元数据及计数；广播格式不变。
- `src/main.cpp`
  - ESP2 同步 Bark 返回后，刷新时钟并接收、判定 ESP3 状态；摘要前刷新新鲜度。
  - 追加 ESP3 和两路接收异常诊断事件、启动版本标识、DIAG_RX 计数；保留旧诊断结构和事件编号。固定 32 条日志，正常每包不写 NVS。
- `test/run_lan_receive.sh`、`test/test_lan_runtime.cpp`、`test/test_lan_main_pass.cpp`
- `test/lan_runtime_stubs/Preferences.h`、`WiFi.h`、`WiFiUdp.h`、`mbedtls/md.h`
  - 有限硬件替身下执行实际 Arduino 接收分支及 main.cpp 连接判定代码，使用真实 HMAC-SHA256。
- `docs/releases/esp1-lan-rx-20260924.1.md`
  - 保存构建、授权刷写、独立校验和运行结果。

## 3. 已作出的关键决策

- TZ 授权执行最小修复计划第 1～3 项，先生成固件，再等待连接 ESP1；随后明确授权刷写。两阶段均已完成。
- 复用原接收器和诊断环形日志，不引入新任务框架、不扩大功能。75 秒离线门槛与温度业务语义保持不变。
- 确定性复现了错误长度包阻塞后续接收，以及 ESP2 同步通知期间到包却仍用旧快照判定 ESP3 的缺陷；这些证明代码缺陷，不证明所有现场告警都由它们造成。
- 用户判断 ESP1 Wi-Fi 无关，作为排查优先级输入；没有 Wi-Fi 事件日志不能绝对排除所有无线或 AP 问题。
- 放弃在本轮修改 Wi-Fi 省电、放宽离线阈值、修改其他设备固件或扩展通知系统。ESP1 自身断网时的告警归因问题未纳入本轮。
- 当前无需要 TZ 立即裁决的新设计事项；下一步是现场观察反馈。

## 4. 验证状态

### 已运行命令（实施阶段，收工未重复执行）

仓库根目录下：

```sh
sh test/run_lan_receive.sh
c++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/extension_lan_state/include test/test_extension_lan_state/test_extension_lan_state.cpp lib/extension_lan_state/src/extension_lan_state.cpp -o .build/extension_lan_state_tests && .build/extension_lan_state_tests
c++ -std=c++17 -Wall -Wextra -Werror -Ilib/grass_lan_state/include test/test_grass_lan_state/test_grass_lan_state.cpp lib/grass_lan_state/src/grass_lan_state.cpp -o .build/grass_lan_state_tests && .build/grass_lan_state_tests
c++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/active_event_snapshot/include -Ilib/heartbeat_contract/include -Ilib/transport_contract/include test/test_daily_summary/test_daily_summary.cpp lib/transport_contract/src/daily_summary.cpp lib/transport_contract/src/transport_contract.cpp -o .build/daily_summary_tests && .build/daily_summary_tests
/Users/tristanzh/.platformio/penv/bin/pio run -e waveshare_esp32s3_n16r8
git diff --check
```

刷写使用 esptool 5.1.0，对核验身份后的目标执行 `write-flash 0x10000 <归档 firmware.bin>`，再独立执行 `verify-flash 0x10000 <同一文件>`；出于标识保护，此处不提供可直接运行的历史端口命令。

### 软件与构建结果

- 修复前：错误长度包恢复、4 包批次处理、旧 main.cpp 的同步 Bark 期间到包场景均出现预期失败。
- 修复后：8 项定向运行时测试通过；现有 ESP2/ESP3 接收器和每日摘要 3 个主机回归程序通过；诊断存储兼容静态检查通过。
- 正式构建通过（24.21 秒），RAM 57,712 字节。镜像 1,365,264 字节，适配现有应用分区。
- 环境 `waveshare_esp32s3_n16r8`；PlatformIO 6.2.0，平台 55.03.36，Arduino 3.3.6。
- 版本 **esp1-lan-rx-20260924.1**。
- SHA-256：`e547e446b042910cef140f37f27476872f5c921fd556f3715168be24463c7919`。

### 已安装与真实运行证据

- 刷写前核验 MCU 身份匹配 ESP1；两个 USB 接口中只操作已核验的 ESP1。
- 写入和独立校验返回 0，分别得到 `Hash of data verified.` 与 `Verification successful (digest matched).`。仅写应用，保留分区表/NVS。
- 100 秒窗口确认版本标识、两路 READY、Wi-Fi 连接及 `heartbeat delivered`。
- ESP2 接受 8 包、ESP3 接受 3 包。期间无断连/恢复通知发送标记，无 Wi-Fi 断开标记；这不等于手机端逐条通知验收。
- 第约 85 秒计数：ESP2 7/7/0、ESP3 2/2/0（received/accepted/rejected）；采样早于窗口结束，不与 100 秒总数混用。
- 旧 23 条 DIAG 事件逐条保留，串口已关闭，无剩余占用。

### 未验证项及手动路径

- 此版本刷写后 Tab5/快捷查询尚未重新获用户确认：查看六缸是否均有正常更新，尤其南美草缸。
- 尚未覆盖过去的多次复发窗口与 24 小时运行。让系统正常运行，如再现，保存 Bark 判定/恢复时间与对应界面状态。
- 受控中断及恢复各一次的实机通知验收未执行，避免收工时主动干扰；若需要，另行明确授权。
- 未运行无关全量测试：影响限定为两路接收合同、主循环连接判定及摘要消费者，已进行相应定向验证。

## 5. 未解决的风险/报错

- 现场根因仍未完全确认：刷写前 23 条日志未覆盖（overwritten=0）。与截图匹配的 ESP2 22:06:35 事件 age=75,024 ms，22:06:39 恢复，接受间隔 79,877 ms，序号跳过 7 帧。
- 这份日志没有 WIFI_DOWN 或阈值 10 秒的 LOOP_GAP；只能支持优先检查发送/投递/接收路径，不能区分未发送、网络丢失或接收拒绝。
- 序号跳过不等同于已确认无线丢包。新版本 RAM 计数仅覆盖本次启动；固定 32 条持久日志会覆盖最早记录，因此复现后应及时保留。
- 本轮不存在已知失败的构建或定向测试；尚缺长期稳定性证据。

## 6. 下一步行动

- [ ] Action-1：先打开 `docs/releases/esp1-lan-rx-20260924.1.md:59` 查看部署证据，确认现场仍运行该版本。
- [ ] Action-2：由 TZ 反馈本次刷写后 Tab5、快捷查询六缸显示及自然运行期间 Bark 是否复现。不要先改阈值或刷其他设备。
- [ ] Action-3：如复现，经授权连接 ESP1，先做脱敏目标/占用检查，再在复位前读取 DIAG；保留旧日志、当前两路计数、事件时间、会话变化和跳号。
- [ ] Action-4：结合接收总数与拒绝数区分“未到达接收器”与“到达但被拒绝”，再决定是否需要 ESP2/ESP3 发送端证据。无证据时保持根因未确认。
- [ ] Action-5：只有获得对应新授权才进行硬件中断验收、进一步修复或刷写。提交/推送按现有 Git 审批路径处理，本次未执行。

## 7. 证据索引

- 治理：项目 `AGENTS.md`，父级 `/Users/tristanzh/agent/AGENTS.md`。
- 收工 Git 检查：`git status --short`、`git diff --stat`、`git diff --name-only`、`git diff --cached --name-only`、`git log --since='midnight' --name-status --oneline`、`git rev-parse HEAD`、限定文件 `git diff`。
- 公开仓库记录：`docs/releases/esp1-lan-rx-20260924.1.md`。
- 受限本地证据：`.build/releases/esp1-lan-rx-20260924.1/` 中的 `runtime-result.json`、`preflash-diag.log`、`postflash-runtime.json`、`write-flash.log`、`verify-flash.log`、`manifest.json`、`source.patch` 和镜像；Git 忽略，目录 0700/文件 0600。镜像含私密配置，不能公开分发。
- 用户确认来源：本对话“已经真机验收确认…所有6个鱼缸…”、“授权你开始实施1～3项…”、“已连接ESP1，授权开始固件刷写”。
- 本文为重启交接，后续以 TZ 人工审阅后的本地实际文本恢复上下文；产品行为及权限仍服从最新明确授权、合同与治理。
