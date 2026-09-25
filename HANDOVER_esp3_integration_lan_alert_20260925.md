# ESP3 接入与断连告警修复交接（2026-09-25）

### 次日启动胶囊

```text
请静默读取并完全理解 `/Users/tristanzh/agent/agent11-fishtank-monitor/HANDOVER_esp3_integration_lan_alert_20260925.md`。

1. 将工作流锁定为“ESP3接入与断连告警修复”，回复第一行使用 `# ESP3接入与断连告警修复 工作流重启`。
2. 开始任何操作前，简述核心卡点、下一步行动和待裁决事项。
3. 在收到本次新授权前，只读查看此 handover 和确认项目根所需的 `AGENTS.md`；不得读设备、运行测试/构建、操作硬件、调用外部系统或写 Git。
```

## 项目与范围

- 权威项目根：`/Users/tristanzh/agent/agent11-fishtank-monitor`。
- 工作流：ESP3 接入与断连告警修复。
- 当前结论：ESP1 的 Bark 由 ESP3 广播包未及时到达 ESP1 接收路径触发。已证明 ESP3 持续发送、电脑旁路能收到，而 ESP1 会连续漏收；尚未区分 AP 对 ESP1 的下行转发与 ESP1 的 Wi-Fi 驱动/UDP 栈。
- 本 handover 仅覆盖 Agent11 接收端与本轮 ESP3 发送观测。温度阈值、75 秒 freshness、TGR1 格式和传输语义没有修改。

## 今日完成

### 固件与诊断

- 前序已构建并刷入 ESP1 诊断固件 `esp1-wifi-ps-none-20260925.1`。运行日志确认 `wifi_power_save=none`，并在 stale/recovery 时输出 RSSI。刷写和构建是本对话前序操作；本收工轮次没有重新构建、刷写或重新校验镜像。
- 前序已刷入 ESP3 发送观测固件 `esp3-tx-observe-20260925.1`。串口持续输出序号、uptime 和 `result=ok`；电脑 UDP 旁路记录同一 TGR1 序号。
- 本收工轮次没有改固件源代码。ESP1 RSSI 诊断的三处未提交改动已在收工时存在：
  - `src/main.cpp`：在 ESP3 stale/recovery 时记录 RSSI。
  - `test/lan_runtime_stubs/WiFi.h`：提供 RSSI 测试替身并捕获诊断标记。
  - `test/test_lan_main_pass.cpp`：断言 stale/recovery 标记及 RSSI。
- 上述未提交改动合计 40 行新增、0 行删除；暂存区为空。不要覆盖、丢弃或顺手重构。

### 监听结果

- 三路监听窗口：2026-09-25 20:46:22 CST 起，2026-09-25 21:57:54 CST 停止。未等满两小时，因为已获得能确认告警直接原因的长时对照样本。
- 主要证据：ESP1 最后收到序号 124 后 stale，`age_ms=75049`、RSSI `-47 dBm`。之后 ESP3 对序号 125–134 持续记录 `result=ok`，电脑旁路收到相同序号；ESP1 连续漏掉 10 包，随后收到序号 135 才恢复，`gap_ms=330368`、`missed_frames=10`、接收计数 `82/82/0`（received/accepted/rejected）。
- 该缺口期间 ESP1 仍收到 ESP2 数据，Bark HTTP 返回 200；这不是 ESP1 整机离线，也不是 ESP1 收到后拒收/校验失败。
- 另有多次复现：例如 stale 序号 86、110、114；RSSI 约 `-45` 至 `-47 dBm`。停采前最后一次 stale 为序号 142，RSSI `-48 dBm`；电脑旁路和 ESP3 已到序号 144，ESP1 最后快照为 ESP3 `87/87/0`、ESP2 `342/342/0`。停止监听前没有看到这次 stale 恢复。
- Bark 记录有 `http status=200`，只证明请求得到 2xx，不等同于手机端实际收件验收。
- 日志仍在本机临时目录：`/tmp/agent11_esp1_rssi.srp0di`、`/tmp/agent11_esp3_rssi.pPw9E9`、`/tmp/agent11_udp35115_rssi.0xAH3h`。三路进程已通过 Ctrl-C 结束；设备未复位、拔插或重新刷写。收工后设备连接状态未复查。

## 根因边界与未决项

- 已确认的直接根因：ESP3→ESP1 的 UDP 广播投递间歇性漏包，超过 75 秒 freshness 后 ESP1 按现有规则触发 stale Bark。ESP3 的发送成功和另一台 LAN 主机收到相同序号，排除了“ESP3 未发送”；ESP1 的 `rejected=0` 排除了应用层收到后拒收。
- 接收代码只在 `grass_udp.parsePacket()` 返回数据报后递增 `received`；因此这批缺失包在进入应用解析前就未见于 ESP1。参见 `lib/grass_lan_state/src/grass_lan_state.cpp` 的 `tick()`。
- 尚未确认的物理/网络组件：AP 对 ESP1 单站的广播转发，还是 ESP1 无线驱动/lwIP/UDP 接收链。ESP2 没有自身 Bark 不能排除这一点，因为 ESP2 的告警路径不同；当前 ESP1 仍能接收 ESP2 数据，也能提交 Bark HTTPS 请求。
- 不要据 RSSI 单点读数断言无线链路完全无误；`-45` 至 `-48 dBm` 只降低“整体信号很弱”的可能性。

## 验证与代码状态

- 本次只读命令：`git status --short`、`git diff --stat`、`git diff --name-only`、`git diff --cached --name-only`、`git log --since=midnight --name-status --oneline`、`git rev-parse HEAD`、`git diff --check`。`git diff --check` 通过。
- HEAD：`d08bd300cbdabd4db167534d614421ecfda9d4c3`。今日历史中可见 `1e067b5`（持久诊断日志）、`0c8d197`（主循环诊断）、`d08bd30`（Wi-Fi 省电策略）。它们已在 HEAD 历史；当前仅上述三个 RSSI 文件有未提交更改。
- 前序运行记录：`sh test/run_lan_receive.sh` 的 7 个 LAN 场景和主循环测试通过；`pio run -e waveshare_esp32s3_n16r8` 构建通过。上述检查未在收工轮次重跑。
- 本收工只新建此 handover 文件；没有新增测试、构建、设备操作或 Git 写入。

## 下一步

1. 先做只读判别：若 AP 能提供 ESP1 客户端的广播/组播转发计数或关联节点信息，核对丢包时 AP 是否仍向 ESP1 转发。不要把 AP 整体故障作为既定结论。
2. 若 AP 无可用计数，设计同一 TGR1 流的受控广播/单播 A/B，或让第二个接收端记录同一序号。单播目标与临时传输路径需先明确；不要擅自改变正式广播合同、75 秒阈值或 freshness 语义。
3. 开始新的跨设备测试前，重新确认各仓库治理、脏状态、设备身份和操作范围。尽管本对话中 TZ 授权必要的诊断固件编码/刷写，新对话仍按启动胶囊先只读；传输行为改变须先明确其产品影响与批准范围。

## 证据来源

- `AGENTS.md`：Agent11 本地治理；handover 记录状态，不替代产品合同。
- Git：HEAD 与收工时的状态/差异见上文；没有提交或推送。
- 设备/旁路：上述三份本机串口与 UDP 日志；实际 Bark 手机收件未单独验收。
- 用户确认：ESP1 与 ESP3 同时接入电脑监听；授权当前问题所需的诊断固件编码/刷写；询问 ESP2 同 Wi-Fi 无 Bark 的差异。
