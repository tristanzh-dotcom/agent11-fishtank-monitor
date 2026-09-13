# 硬件接入前四项缺口收尾审计

范围：TZ 授权修复的告警文案、本地 Bark 开关、构建版本、文档错误。采用 Level 2 有界验证。

- 主缸标题/正文含“包包缸”；主缸与小缸故障 opened/reminder 显示“无有效读数”，resolved 显示有效恢复温度。共用格式化逻辑，保留主缸 fingerprint、group 和 level 语义。
- 人工逐路径审查 `src/main.cpp`：本地 Bark 关闭时不入小缸队列、不生成日报，所有本地 Bark 发送入口均有开关条件；引擎和心跳不受该开关影响。未模拟真实网络调用，板上开关行为仍需现场验证。云端原有 Bark 路径不受此本地开关控制。
- 平台固定为 pioarduino 55.03.36 发布包；README 记录 Core 6.2.0 / Python 3.12 基线。当前 pip check 无冲突，本轮没有修改全局 Python 环境。版本固定不代表离线或逐字节可复现构建，依赖下载仍需要公开包源。
- 保留备用探头时另备三支，投入备用时另备两支；修正 README 传输测试漏掉 scoped_event_outbox.cpp 的命令，上传示例要求明确串口。

## 已执行验证

1. README 中传输测试的 g++ C++17 / -Wall -Wextra -Werror 命令：新增断言先在旧实现失败（主缸名称），修复后通过；覆盖主缸和小缸三种故障状态、主缸完整 JSON。
2. README 中 daily_summary_tests 构建/运行命令：通过。
3. `/Users/tristanzh/.platformio/penv/bin/python -m pip check`：No broken requirements found。
4. `/Users/tristanzh/.platformio/penv/bin/pio run -e waveshare_esp32s3_n16r8`：SUCCESS，平台 55.3.36 / Arduino 3.3.6；RAM 53520 bytes，Flash 1318315 bytes。OneWire 2.3.8 仍有两条既有 #undef 警告。
5. `sh test/verify_docs.sh` 和 `git diff --check`：通过。

结论：四项缺口已处理。此次差异未修改温度策略、ROM、SCF、心跳合同或 Web；不需要全云端回归。代码审查和构建不等同于板上运行证据。

未执行：上传、真实 Bark、现场 ROM 识别、总线稳定性、校准及 48–72 小时观察；这些仍属于硬件接入阶段。未提交或推送 Git。
