# 单缸水温监控 MVP

这是第一缸的微雪 `ESP32-S3-DEV-KIT-N16R8-M` 固件：两个 DS18B20 水温探头、
设备直连 Bark HTTPS 告警、默认关闭的可选 MQTT，以及完全在设备本地执行的温度判断。
它不依赖常驻电脑或外部云平台，也不控制加热棒、水泵、插排或其他市电设备。

## 已实现的告警策略

显示缸探头是鱼只安全的唯一判断依据，回水仓探头只做循环诊断。采样间隔为 30 秒。

| 场景 | 触发 | 级别 |
| --- | --- | --- |
| 低温 | 显示缸 `<23.5°C` 持续 15 分钟 | N2 |
| 严重低温 | 显示缸 `<22.5°C` 持续 5 分钟 | N3 |
| 高温 | 显示缸 `>27.5°C` 持续 15 分钟 | N2 |
| 严重高温 | 显示缸 `>28.5°C` 持续 5 分钟 | N3 |
| 温变过快 | 30 分钟内绝对变化 `≥1.0°C` / `≥2.0°C` | N2 / N3 |
| 循环异常 | 双探头温差 `>0.8°C` 持续 10 分钟 | N2 |
| 探头失效 | 连续两次显示缸读数无效 | N2 |

高温恢复要求 `≤26.5°C` 持续 15 分钟；低温恢复要求 `≥24.5°C` 持续 15 分钟。
未恢复事件每 60 分钟最多提醒一次。夏季自然稳定的 26–27°C 不会触发高温事件。

## 首次配置与编译

1. 安装 PlatformIO Core：`python3 -m pip install --user platformio`。
2. 复制 `include/secrets.example.hpp` 为本机 `include/secrets.hpp`。只在该忽略文件中填写
   Wi-Fi、Bark Device Key 和 `api.day.app` 的 CA PEM；不要把值发到聊天、日志或截图。
3. 保持 `include/config.hpp` 的 `bark_enabled = true` 和 `mqtt_enabled = false`。
   MQTT 不是 MVP 前提；仅在未来明确需要时才启用并填写其凭据与 CA。
4. 首刷可让两个 DS18B20 ROM 地址保持全零。串口会打印每只探头的 `index` 和 8 字节
   ROM；确认物理角色后固定到 `main_tank_sensor` 与 `sump_return_sensor`。
5. 用 USB-C 数据线连接开发板，执行：

```sh
/Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8 -t upload
```

开发板通过板载 CH343/CH334 将 UART 与 USB 接到同一个 USB-C。固件保留 UART0
`Serial`，不启用原生 USB CDC 重定向。先确认实际串口，再以 115200 baud 观察输出：

```sh
/Users/tristanzh/Library/Python/3.9/bin/pio device list
/Users/tristanzh/Library/Python/3.9/bin/pio device monitor -b 115200
```

首次上电前先阅读 [硬件安装](docs/hardware-installation.md)。接入水体后做 48–72 小时
只监控浸泡验证，不改动原有加热系统。

## Bark 主路径与可选 MQTT

Bark 是 MVP 默认主路径。设备将事件以 JSON POST 发送到固定端点
`https://api.day.app/push`；Device Key 只放在经过 TLS 加密的请求体中，不出现在 URL、
串口日志或事件文本。N2 使用 `timeSensitive`，N3 使用 `critical`。

Bark 与 MQTT 各有独立 16 条 RAM 队列。Bark 成功仅移除 Bark 队首，MQTT 成功仅移除
MQTT 队首；MQTT 默认关闭，其未配置、连接失败或发布失败不会阻塞 Bark。队列不跨断电
持久化，因此复位会丢失尚未投递的事件。设备断电或完全离线时也无法自行发出 Bark，
这是无外部在线监控的 MVP 已知边界。

如果显式启用 MQTT，原遥测和事件 Topic/JSON 契约保持不变。`cloud/bark-forwarder/`
仅作为未部署的历史可选适配器保留，不需要为当前 MVP 创建任何云资源。

## 本地验证

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_simulation.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_simulation_tests && ./.build/temperature_simulation_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include test/test_temperature_engine/test_config.cpp -o .build/config_tests && ./.build/config_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && ./.build/transport_contract_tests
node --test cloud/bark-forwarder/index.test.mjs
sh test/verify_docs.sh
/Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8
```

## 安全边界

- 控制盒必须放在底柜外侧，传感器线做滴水弯。
- 不在潮湿底柜内放 ESP32、USB 电源或插排；不接继电器和市电。
- 主缸探头是安全报警依据，底滤探头只做循环诊断。
- 禁止在源码、串口日志、截图、URL 或事件中输出 Wi-Fi 密码、MQTT 密码或 Bark key。
