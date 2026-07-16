# 可选的历史阿里云函数计算 Bark 转发器

当前 MVP 已改为 ESP32-S3 直接 HTTPS 调 Bark，本目录不再是必需部署路径，也未部署。
只有未来显式启用 MQTT/阿里云链路时才参考以下说明。

此目录是尚未部署的 Node.js 函数计算事件处理器。它接收设备发布到
`/${productKey}/${deviceName}/user/aquarium/events` 的 JSON，经 IoT Platform 规则转发后，使用
环境变量中的 Bark key 发送 HTTPS POST。源码不含任何密钥。

事件至少必须包含 `event_type`、`state`、`severity`、`timestamp_ms`、`display_c` 和 `event_id`。

## 部署前配置

1. 在阿里云 IoT Platform 建立规则：匹配上述 events 主题，将完整 JSON 事件转发到函数计算。
2. 创建 Node.js 18 或更高版本的事件函数，入口设置为 `index.handler`，上传本目录的 `index.mjs`。
3. 为函数设置环境变量：
   - `BARK_DEVICE_KEY`：Bark App 的设备 key；
   - `AQUARIUM_ID`：建议 `tank01`。
4. 以一条 N2 和一条 N3 模拟事件测试。不要在函数日志中打印环境变量、完整请求体或设备 key。

`index.mjs` 返回的是已投递事件的语义指纹，不是持久化去重存储。MVP 的去重由设备端
opened/reminder/resolved 状态机和 `event_id` 关联承担；如果未来需要跨设备/跨重启去重，应添加受控的持久化存储后再变更架构。
