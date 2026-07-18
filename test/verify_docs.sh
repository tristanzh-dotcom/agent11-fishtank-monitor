#!/usr/bin/env sh
set -eu

for heading in \
  "# 单缸水温监控 MVP" \
  "## 首次配置与编译" \
  "# 第一缸硬件安装与上电流程" \
  "## 安装位置与验证"; do
  rg -F --quiet "$heading" README.md docs/hardware-installation.md
done

for rom_role in "main_tank_sensor" "sump_return_sensor"; do
  if ! rg -F --quiet -- "$rom_role" \
    include/config.hpp src/ds18b20_reader.cpp README.md \
    HANDOVER_SOFTWARE_20260716.md docs/hardware-installation.md; then
    echo "missing fixed-ROM role: $rom_role" >&2
    exit 1
  fi
done

if rg -P --quiet '(?<![A-Za-z0-9_])(display_sensor|return_sensor)(?![A-Za-z0-9_])' \
  include/config.hpp src/ds18b20_reader.cpp README.md \
  HANDOVER_SOFTWARE_20260716.md docs/hardware-installation.md; then
  echo "current software or documentation still uses retired ROM role names" >&2
  exit 1
fi

for s3_contract in \
  "ESP32-S3-DEV-KIT-N16R8-M" \
  "default_envs = waveshare_esp32s3_n16r8" \
  "board = esp32-s3-devkitc-1" \
  "board_upload.flash_size = 16MB" \
  "board_build.partitions = default_16MB.csv" \
  "board_build.arduino.memory_type = qio_opi" \
  "-DBOARD_HAS_PSRAM"; do
  if ! rg -F --quiet -- "$s3_contract" README.md docs/hardware-installation.md platformio.ini; then
    echo "missing S3 contract: $s3_contract" >&2
    exit 1
  fi
done

for bark_primary_contract in \
  "ESP32-S3 → HTTPS → Bark" \
  "Bark 是 MVP 默认主路径" \
  "MQTT 默认关闭" \
  "https://api.day.app/push" \
  "bool bark_enabled = true" \
  "bool mqtt_enabled = false"; do
  if ! rg -F --quiet -- "$bark_primary_contract" \
    HANDOVER_SOFTWARE_20260716.md README.md include/config.hpp src/bark_notifier.cpp; then
    echo "missing Bark-primary contract: $bark_primary_contract" >&2
    exit 1
  fi
done

for heartbeat_contract in \
  "bool heartbeat_enabled = true" \
  "heartbeat_interval_ms = 300000U" \
  "devices/tank01/state.json" \
  "X-Aquarium-Signature" \
  "腾讯云心跳" \
  "15 分钟"; do
  if ! rg -F --quiet -- "$heartbeat_contract" \
    include/config.hpp include/secrets.example.hpp src/main.cpp \
    src/heartbeat_notifier.cpp README.md HANDOVER_SOFTWARE_20260716.md \
    cloud/tencent-scf/README.md; then
    echo "missing Tencent heartbeat contract: $heartbeat_contract" >&2
    exit 1
  fi
done

if rg -F --quiet \
  "固件内的 Bark 仅作临时可选兜底" \
  HANDOVER_SOFTWARE_20260716.md README.md docs/hardware-installation.md; then
  echo "current documentation still treats direct Bark as a fallback" >&2
  exit 1
fi

if rg -F --quiet \
  "阿里云设备离线规则仍是正式运行时的必要保护" \
  HANDOVER_SOFTWARE_20260716.md README.md docs/hardware-installation.md; then
  echo "current documentation still requires Alibaba Cloud for the MVP" >&2
  exit 1
fi

if ! rg -F --quiet "底柜外侧" \
  HANDOVER_HARDWARE_20260716.md README.md docs/hardware-installation.md; then
  echo "hardware placement must be documented as outside the cabinet" >&2
  exit 1
fi

for final_hardware_contract in \
  "DFRobot KIT0021" \
  "WAGO 221-413" \
  "RVV 三芯 0.3平方" \
  "2.0USB-C" \
  "ANENG 616" \
  "8PK-3001D"; do
  if ! rg -F --quiet -- "$final_hardware_contract" \
    HANDOVER_HARDWARE_20260716.md docs/hardware-installation.md \
    docs/hardware-build-guide.md docs/hardware-build-guide.html; then
    echo "missing final hardware contract: $final_hardware_contract" >&2
    exit 1
  fi
done

if rg -F --quiet "KF301" \
  HANDOVER_HARDWARE_20260716.md docs/hardware-installation.md \
  docs/hardware-build-guide.md docs/hardware-build-guide.html; then
  echo "current hardware documentation still references retired KF301 terminals" >&2
  exit 1
fi

for guide_heading in \
  "# 第一缸 ESP32-S3 温度监控：无焊接制作手册" \
  "第一缸 ESP32-S3 温度监控" \
  "GPIO4-DATA" \
  "48–72 小时"; do
  if ! rg -F --quiet -- "$guide_heading" \
    docs/hardware-build-guide.md docs/hardware-build-guide.html; then
    echo "missing hardware guide contract: $guide_heading" >&2
    exit 1
  fi
done

for beginner_guide in \
  docs/hardware-build-guide.md \
  docs/hardware-build-guide.html; do
  for beginner_marker in \
    "黑表笔" \
    "COM" \
    "USB 已拔掉" \
    "立即停止" \
    "Type A" \
    "Type B" \
    "A-VCC" \
    "A-GND" \
    "A-DATA" \
    "总-VCC" \
    "总-GND" \
    "总-DATA" \
    "第十个 WAGO" \
    "15 分钟" \
    "120 分钟" \
    "48–72 小时" \
    "视频只作视觉补充"; do
    if ! rg -F --quiet -- "$beginner_marker" "$beginner_guide"; then
      echo "missing beginner marker in $beginner_guide: $beginner_marker" >&2
      exit 1
    fi
  done

  youtube_link_count=$(
    rg -o 'https://www\.youtube\.com/watch\?[^)"< ]+' "$beginner_guide" \
      | wc -l \
      | tr -d ' '
  )
  if [ "$youtube_link_count" -lt 5 ]; then
    echo "fewer than five direct YouTube links in $beginner_guide" >&2
    exit 1
  fi
done

if rg -F --quiet \
  -e "按常规接线" \
  -e "自行选择一个 GPIO" \
  -e "万用表测量市电" \
  docs/hardware-build-guide.md docs/hardware-build-guide.html; then
  echo "beginner guide contains prohibited shorthand or mains-meter wording" >&2
  exit 1
fi

if rg --quiet '底柜内的较高|底柜内较高|底柜内高处' \
  HANDOVER_HARDWARE_20260716.md README.md docs/hardware-installation.md; then
  echo "hardware placement still permits installation inside the cabinet" >&2
  exit 1
fi

if rg -i --quiet 'ESP32[-_ ]?C3|esp32c3|c3-devkit' \
  README.md docs/hardware-installation.md platformio.ini; then
  echo "current documentation or build config still references ESP32-C3" >&2
  exit 1
fi

if rg --glob '!include/secrets.hpp' --glob '!include/secrets.example.hpp' \
  'replace-with-wifi-password|replace-with-mqtt-password' src include; then
  echo "placeholder secret leaked outside secret files" >&2
  exit 1
fi

if rg -F --quiet "setInsecure()" src include lib; then
  echo "firmware must not disable TLS certificate verification" >&2
  exit 1
fi

echo "documentation checks passed"
