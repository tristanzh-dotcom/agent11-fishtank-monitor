# 温度告警双时间实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**目标：** 按已确认的双时间设计，统一设备直推与云端主缸温度 Bark 文案；保留现有告警判定、队列、重试、等级、心跳和公开接口。

**范围：** 设备 Bark 待发记录附带一次事件判定时间，云端温度通知增加缺失事件时间和本次云端发送发起时间；不扩展心跳合同，不改变离线/恢复通知、日报、快捷查询、MQTT、COS 或 Tab5。

## 步骤

- [x] 用 C++ transport/delivery 与 Node Bark 测试锁定状态、读数、北京时间、缺失时间和重试边界。
- [x] 在设备 Bark 专用待发记录中原子保存事件时间，改造最小文案格式化和发送时钟读取；保留 MQTT 队列与事件引擎。
- [x] 在云端 Bark notifier 注入时钟，温度分支只消费事件读数，统一标题/正文并保留离线/恢复分支。
- [x] 运行受影响的主机测试和固件目标构建；检查无密钥泄露、无接口回归。
- [x] 生成 SCF 上传包，部署到现有 `fishtank-monitor` 函数并做最小受控运行验证；不刷写固件、不做现场验收。

## 验证

已通过 C++ transport contract、Node Bark/heartbeat/runtime 测试、PlatformIO 两个目标构建、包内容检查和部署后未授权最小调用验证。发送时间仅表示发送流程发起；云端事件时间缺失显示“事件时间不可用”。
