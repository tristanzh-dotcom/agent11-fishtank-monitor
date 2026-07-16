# Waveshare ESP32-S3 N16R8 Adaptation Implementation Plan

> **For agentic workers:** Execute this plan inline in the current task. Sub-agent execution is not authorized for this task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and document the aquarium firmware for the Waveshare `ESP32-S3-DEV-KIT-N16R8-M` instead of the former ESP32-C3 target.

**Architecture:** Use PlatformIO's `esp32-s3-devkitc-1` board definition because Waveshare documents pin compatibility with that board. Override the generic board's 8 MB/no-PSRAM defaults for the Waveshare module's 16 MB Quad Flash and 8 MB Octal PSRAM. Keep DS18B20 OneWire on GPIO4 and keep `Serial` on UART0 through the onboard CH343 bridge.

**Tech Stack:** PlatformIO Core 6.1.19, PlatformIO Espressif32 7.0.1, Arduino-ESP32 2.0.17, C++17, Node.js tests, shell documentation checks.

## Global Constraints

- Target board: Waveshare `ESP32-S3-DEV-KIT-N16R8-M`.
- Storage: 16 MB Quad SPI Flash and 8 MB Octal SPI PSRAM.
- OneWire data remains on GPIO4 with an external 4.7 kOhm pull-up to 3V3.
- Serial logs remain at 115200 baud on UART0 through the onboard CH343 bridge; do not redirect `Serial` to native USB CDC.
- Preserve all approved monitoring behavior and cloud message contracts.
- Do not add credentials, Bark keys, relay control, or any mains-voltage control.
- This directory is not a Git repository; do not perform Git write operations.

---

### Task 1: Lock the S3 PlatformIO Contract

**Files:**
- Modify: `platformio.ini`
- Modify: `test/verify_docs.sh`

**Interfaces:**
- Consumes: PlatformIO board ID `esp32-s3-devkitc-1`.
- Produces: environment `waveshare_esp32s3_n16r8` with 16 MB flash, `default_16MB.csv`, and `qio_opi` PSRAM support.

- [x] **Step 1: Extend the deterministic documentation/configuration check**

Require the S3 environment name, compatible board ID, 16 MB flash override, 16 MB partition table, OPI PSRAM memory type, and `BOARD_HAS_PSRAM`; reject current-user documentation that still identifies the target as C3.

- [x] **Step 2: Run the check and confirm it fails against the C3 configuration**

Run: `sh test/verify_docs.sh`

Expected: failure because `platformio.ini` still names `esp32c3`.

- [x] **Step 3: Replace the C3 environment with the S3 environment**

Use this contract:

```ini
[platformio]
default_envs = waveshare_esp32s3_n16r8

[env:waveshare_esp32s3_n16r8]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
board_upload.flash_size = 16MB
board_upload.maximum_size = 16777216
board_build.partitions = default_16MB.csv
board_build.arduino.memory_type = qio_opi
monitor_speed = 115200
build_unflags = -std=gnu++11
build_flags =
  -std=gnu++17
  -DBOARD_HAS_PSRAM
```

Keep the existing dependency versions unchanged.

- [x] **Step 4: Run the deterministic check and inspect resolved PlatformIO configuration**

Run: `sh test/verify_docs.sh`

Run: `/Users/tristanzh/Library/Python/3.9/bin/pio project config --json-output`

Expected: the check passes and the resolved default environment is `waveshare_esp32s3_n16r8`.

### Task 2: Align Current User Documentation

**Files:**
- Modify: `README.md`
- Modify: `docs/hardware-installation.md`

**Interfaces:**
- Consumes: the PlatformIO environment from Task 1 and the approved alarm policy.
- Produces: S3-specific build, upload, serial, wiring, and safety instructions.

- [x] **Step 1: Replace current C3 terminology and commands**

Use the exact board name `ESP32-S3-DEV-KIT-N16R8-M` and environment `waveshare_esp32s3_n16r8`. Explain that the single USB-C exposes the onboard CH343 UART bridge and that the serial monitor remains 115200 baud.

- [x] **Step 2: Correct the sensor-fault severity drift**

Change the README sensor-fault row from N3 to the approved and implemented N2 level.

- [x] **Step 3: Preserve the GPIO4 wiring and explain why it is valid**

Document GPIO4 as a normal exposed S3 GPIO and retain the 4.7 kOhm pull-up to 3V3.

- [x] **Step 4: Run documentation verification**

Run: `sh test/verify_docs.sh`

Expected: `documentation checks passed`.

### Task 3: Execute Full Regression and S3 Build

**Files:**
- Verify only: all production and test files.

**Interfaces:**
- Consumes: the S3 build configuration and unchanged firmware contracts.
- Produces: executable test evidence and an S3 firmware binary under `.pio/build/waveshare_esp32s3_n16r8/`.

- [x] **Step 1: Run both temperature engine host tests**

Run the two `g++ -std=c++17 -Wall -Wextra -Werror` commands from `HANDOVER_SOFTWARE_20260716.md`.

Expected: both executables exit 0.

- [x] **Step 2: Run configuration and transport host tests**

Run the configuration test and transport-contract test from `HANDOVER_SOFTWARE_20260716.md`.

Expected: both executables exit 0.

- [x] **Step 3: Run the Bark forwarder and documentation tests**

Run: `node --test cloud/bark-forwarder/index.test.mjs`

Run: `sh test/verify_docs.sh`

Expected: all Node tests pass and documentation checks pass.

- [x] **Step 4: Build the S3 firmware**

Run: `/Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8`

Expected: successful ESP32-S3 build using the 16 MB partition table and OPI PSRAM SDK configuration.

- [x] **Step 5: Inspect the firmware image contract**

Inspect the build output and generated image metadata to confirm ESP32-S3, 16 MB flash configuration, and the final RAM/Flash usage. Do not claim hardware USB, PSRAM, GPIO, or sensor operation until the physical board is tested.
