# Agent11 Fish-Tank Monitor：远程五缸温度快捷查询交接

### ☀️ 次日启动胶囊 (Boot Prompt)

请在明天开启新对话时，直接复制以下指令发给系统：

~~~text
请静默读取并完全理解当前目录下的 `HANDOVER_remote_five_temperature_shortcut_20260912.md`。
*注：“静默读取”指不要逐段复述全文，只需精准提炼核心卡点、下一步行动和需要 TZ 确认的事项。*

1. 请将本对话的逻辑分支锁定为：【远程五缸温度快捷查询】，并在你回复的第一句话使用 Markdown 的 H1 标题 (`# 远程五缸温度快捷查询 工作流重启`) 输出，以便系统自动重命名此对话。
2. 请在执行任何操作前，简要复述当前的【核心卡点】与【下一步行动】。
3. 仅当本次交接确实存在需要 TZ 裁决的未决事项时，才在回复中用简洁自然语言说明该事项；不得把固定确认句追加到普通对话或无待决事项的回复末尾。
4. 在收到 TZ 明确授权前，仅允许以只读方式读取指定 handover，以及确定其权威项目根所必需的 `AGENTS.md`；可据此提炼核心卡点、下一步行动和待确认事项。不得修改文件、运行测试/构建/服务、调用外部系统、操作硬件或执行 Git 写入，也不得推进方案或扩大任务范围。
~~~

---

## 1. 第一性原理与项目上下文

### 当前项目根

~~~text
/Users/tristanzh/agent/agent11-fishtank-monitor
~~~

### 当前工作流主题

~~~text
远程五缸温度快捷查询
~~~

### 核心目的

在既有 ESP32 五路采样、Bark 告警和定时日报基础上，让用户在 iPhone 15 Pro Max
通过一个手动“快捷指令”，即使手机不在家庭 Wi-Fi 内，也能读取最近一次五个温度点的
日报格式纯文本。允许约 5 分钟滞后；本功能当前仍处于“设计已确认、待实现”状态。

### 当前上下文边界

- 五个温度点为包包缸主缸、包包缸回水缸、老四缸、小黑缸、毛毛缸；主缸和回水缸在显示上属于同一套鱼缸。
- 计划复用约 30 秒采样和约 5 分钟心跳，仅增加私有温度摘要读取能力。
- 计划使用既有腾讯 SCF/COS、既有只读 Bearer Token 和一个精确的 HTTPS GET 路径。
- 不进入 Web Agent 页面，不修改 FishTankStateV1、既有 /state、Bark 告警、09:00/18:00 定时日报或 Tab5。
- 不增加原生 iOS App、历史数据、图表、自动化通知、端口转发、设备控制或新的云服务。

## 2. 今日完成事项

### 修改文件清单

~~~text
- docs/superpowers/specs/2026-09-12-remote-five-temperature-shortcut-design.md
  - 新建并按复审意见修订远程五缸温度快捷查询设计。
  - 当前为未跟踪文件，尚未提交。
  - 证据来源：git status --short；文件内容复核；用户明确确认十分钟过期规则。
~~~

本轮没有修改代码、SCF、快捷指令、设备配置，也没有进行云端部署或刷写。工作区还
存在此前多缸/Bark、日报、Tab5 相关的未提交改动；这些改动属于既有工作流，未在本轮
交接中重新归因或清理。

### 已确定的方案逻辑（不是已完成的代码实现）

- ESP32 在既有心跳时，将同一采样时刻生成的五缸日报正文作为可选
  temperature_snapshot 上传；包含有效的 sampled_at_ms 和 summary_text。
- 新摘要纳入现有原始 JSON 哈希/HMAC 范围；摘要最多 768 个 UTF-8 字节，整体心跳仍
  受 2048 字节限制。超限时省略整个可选摘要，不截断正文、不丢弃既有事件。
- SCF 增加私有只读路径：
  GET /api/v1/devices/tank01/temperature-summary。
- 查询成功返回 text/plain; charset=utf-8，并设置 Cache-Control: no-store；沿用既有
  STATE_READ_TOKEN 的 Bearer 认证。
- 采样年龄超过 10 分钟时，只显示“温度数据暂未更新，最后采样时间：……”；不展示旧
  温度。设备已判定离线时显示“设备已离线，温度信息暂不可用。”，同样不展示旧温度。
- 没有摘要时显示“暂无温度数据，请稍后重试。”；手机快捷指令只负责 GET 和显示纯文本，
  不在手机侧解析或缓存温度。

## 3. 已作出的关键决策

~~~text
- 决策：采用“既有心跳携带私有摘要 + 既有 SCF/COS 最新状态 + 新增只读 GET + iPhone 手动快捷指令”。
  原因：最大程度复用现有设备联网、HMAC、云端状态和只读认证，满足外地蜂窝网络查询及约 5 分钟滞后。
  放弃的替代方案：家庭 VPN/局域网直连，以及家庭路由器端口转发。
  证据来源：用户确认按该方向生成设计方案；设计文档第 4、5 节。

- 决策：采样超过 10 分钟不返回旧温度，只返回最后采样时间；设备离线也只返回不可用提示。
  原因：避免把过期或离线数据误认为当前温度。
  放弃的替代方案：继续展示旧日报或把云端读取时间当作采样时间。
  证据来源：用户明确接受的复审结论；设计文档第 6 节。

- 决策：远程能力不扩展 Web Agent、FishTankStateV1、Bark 和 Tab5。
  原因：本期只解决手机快捷指令单一只读查询需求，保持现有功能边界。
  放弃的替代方案：新增 Web 页面、原生 App、独立推送/历史服务或 Tab5 联动。
  证据来源：用户的最小化要求及设计文档第 2、7、10 节。

- 决策：当前不能把设计文档视为实现完成，也不能把既有五探头刷写结果视为远程查询验收。
  原因：远程心跳字段、SCF 路由、快捷指令和蜂窝网络链路尚未执行验证。
  证据来源：当前 git 状态、设计文档状态“待实现”、此前工作流验证记录。
~~~

## 4. 验证状态

### 本轮已运行命令

~~~bash
git status --short
git diff --stat
git diff --name-only
git diff --cached --name-only
git log --since=midnight --name-status --oneline
git log -1 --oneline
git diff --check
~~~

### 本轮验证结果

~~~text
- git status --short：工作区有既有未提交改动，远程设计文档显示为未跟踪；无暂存文件。
- git diff --stat / --name-only：确认既有跟踪文件改动清单；未将其误记为本轮远程代码实现。
- git log --since=midnight --name-status --oneline：无今日提交记录。
- git log -1 --oneline：当前最近提交为已有的 selected repo changes 提交，不是本轮远程功能实现。
- git diff --check：无输出，未发现跟踪差异中的空白错误。
- 远程五缸功能主机测试、SCF 测试、目标构建、云端部署、快捷指令和蜂窝网络验收：未运行。
~~~

### 此前既有五探头/Bark 范围的证据（不等于本轮远程功能验证）

此前已在临时的非 Tab5 刷写副本中运行并通过：

~~~bash
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_daily_summary/test_daily_summary.cpp lib/transport_contract/src/daily_summary.cpp lib/transport_contract/src/transport_contract.cpp -o .build/daily_summary_tests && ./.build/daily_summary_tests
# daily summary tests passed

g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/scoped_event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && ./.build/transport_contract_tests
# transport contract tests passed

/Users/tristanzh/.platformio/penv/bin/pio run -e waveshare_esp32s3_n16r8
/Users/tristanzh/.platformio/penv/bin/pio run -e waveshare_esp32s3_n16r8 -t upload --upload-port /dev/cu.usbmodem5B910349491
~~~

上述目标构建和刷写成功，短串口检查确认设备运行及三个新增探头绑定有效；该证据不覆盖
尚未实现的远程摘要字段、SCF 新路由、iPhone 快捷指令或蜂窝网络读取。唯一 USB 连接为
ESP32，后续不应重复询问端口；刷写仍需保持非 Tab5 固件边界并单独核对构建产物。

### 未验证项与次日手动验证路径

1. 先完成并审阅固件/SCF 实现，再运行对应的最小主机合同测试和 Node 测试。
2. 构建目标固件；确认不引入 Tab5 依赖后，只有在 TZ 明确授权时才刷写唯一 ESP32。
3. 部署兼容旧固件的 SCF 版本，回归既有 /state、旧固件心跳和只读认证。
4. 在 iPhone 上配置“获取 URL 内容（GET）+ Authorization Bearer + 显示结果”，关闭 Wi-Fi
   后用蜂窝网络实测五个温度点、采样时间以及离线/超过十分钟提示。
5. 远程查询验收与 Bark、定时日报、Web Agent、Tab5 和 48–72 小时现场观察分开记录。

## 5. 未解决的风险/报错

~~~text
- 风险：远程查询当前尚无固件摘要字段、SCF 路由或 iPhone 快捷指令实现。
  当前状态：设计文档已确认，状态为待实现。
  影响范围：外地蜂窝网络暂不能取得新增的五缸摘要；既有本地采样、Bark 和日报不应因此改变。
  建议下一步：待 TZ 明确授权后按设计文档逐层实现并验证。

- 风险：这是一个新的私有跨层字段，序列化、HMAC、大小上限、旧固件兼容和快照清除规则必须保持一致。
  当前状态：设计已规定，代码尚未验证。
  影响范围：实现错误可能导致心跳拒绝、旧摘要残留或返回过期温度。
  建议下一步：优先做字段/路由的聚焦合同测试，不改公共状态合同。

- 报错：本轮未发现新的构建或测试报错。
~~~

## 6. 下一步行动 (原子化设计)

- [ ] **Action-1 (授权边界)**：由 TZ 明确授权远程快捷查询的代码实现、SCF 部署、非 Tab5 固件刷写及现场验收；授权前只读审阅。
- [ ] **Action-2 (文件定位)**：打开 docs/superpowers/specs/2026-09-12-remote-five-temperature-shortcut-design.md，并定位现有心跳 C++、SCF 请求校验/状态存储及只读路由文件。
- [ ] **Action-3 (固件合同)**：只增加可选 temperature_snapshot，复用现有日报格式化逻辑，覆盖时间有效性、UTF-8/2048 字节边界、超限省略及签名输入测试。
- [ ] **Action-4 (SCF 路由)**：实现精确 GET 路径、旧固件兼容、离线/无摘要/未来时间/十分钟边界判断，以及 text/plain/no-store 响应；保持 /state 不变。
- [ ] **Action-5 (验证)**：运行新增和既有聚焦测试、git diff --check，再运行目标构建并检查无 Tab5 依赖。
- [ ] **Action-6 (部署与硬件)**：仅在对应授权后部署云端并刷写唯一 ESP32；记录软件构建、上传、安装、校准和持续观察为独立证据。
- [ ] **Action-7 (手机验收)**：配置一个手动快捷指令，用蜂窝网络完成一次读取；确认五个温度点、采样时间和过期/离线不展示旧温度。

## 7. 证据索引

~~~text
- AGENTS.md：/Users/tristanzh/agent/agent11-fishtank-monitor/AGENTS.md；项目边界、凭据保护及分层验收要求。
- 设计文档：docs/superpowers/specs/2026-09-12-remote-five-temperature-shortcut-design.md。
- git status / diff / log：本次收工前在权威项目根执行；今日提交为空，工作区存在既有未提交改动。
- 用户确认：接受“超过十分钟只显示最后采样时间、离线显示不可用且不展示旧温度”；此前确认按快捷指令、约五分钟滞后、最小单一功能设计。
- 此前实现证据：临时非 Tab5 副本的日报/传输主机测试、PlatformIO 构建、唯一 ESP32 刷写及短串口运行检查；不覆盖本轮远程功能。
- 未完成证据：没有 SCF 部署记录、iPhone Shortcut 配置记录或蜂窝网络返回记录。
~~~

