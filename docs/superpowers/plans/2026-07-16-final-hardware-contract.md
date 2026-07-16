# Final Hardware Contract Implementation Plan

> **For agentic workers:** Execute this plan inline in the current task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align the S3 temperature-monitor MVP documentation and deterministic checks with the final purchased hardware while preserving the existing direct-Bark software path.

**Architecture:** The existing `ESP32-S3 → HTTPS → Bark` event path, ROM-based dual-probe roles, GPIO4 OneWire bus, and optional MQTT adapter remain unchanged. Current hardware documentation becomes the source of truth for DFRobot KIT0021 probes, WAGO 221-413 distribution, RVV 0.3 mm2 extension wire, and the USB-C data panel lead.

**Tech Stack:** C++17 host tests, Node.js tests, PlatformIO ESP32-S3 build, shell documentation contract check.

## Global Constraints

- Board: Waveshare `ESP32-S3-DEV-KIT-N16R8-M`, 16 MB Flash and 8 MB PSRAM.
- Sensors: three-wire, non-parasitic DFRobot `KIT0021`; install two and keep one spare.
- OneWire: two installed probes share GPIO4 with a 4.7 kOhm pull-up to 3V3.
- MVP primary transport: direct HTTPS Bark; MQTT stays default-off and optional.
- No Bark key, Wi-Fi password, cloud credential, relay, soldering instruction, or mains-voltage control.
- Hardware lives outside the cabinet in a dry location.

---

### Task 1: Lock the Final Purchase Contract

**Files:**
- Modify: `HANDOVER_HARDWARE_20260716.md`
- Modify: `docs/hardware-installation.md`
- Modify: `test/verify_docs.sh`

**Interfaces:**
- Consumes: the final procurement decision in the current task.
- Produces: current documentation that names the actual probe, connector, cable and USB data extension; rejects the retired KF301 assumption.

- [x] **Step 1: Extend the documentation contract check before changing the hardware wording**

Require these literal strings in current documentation: `DFRobot KIT0021`, `WAGO 221-413`, `RVV 三芯 0.3平方`, and `2.0USB-C`; fail when `KF301` appears in the current handover or installation guide.

- [x] **Step 2: Run the documentation check and observe the expected pre-change failure**

Run: `sh test/verify_docs.sh`

Expected: failure for missing `DFRobot KIT0021`.

- [x] **Step 3: Replace the retired purchasing and wiring assumptions**

Document three DFRobot KIT0021 probes, with two installed and one spare; replace KF301 with WAGO 221-413; document the 5 m RVV three-core 0.3 mm2 extension wire; and document the 0.3 m waterproof USB-C male-to-female USB 2.0 data panel lead. Retain GPIO4, 3V3 pull-up, ROM binding, and no-solder/no-mains boundaries.

- [x] **Step 4: Re-run the documentation contract check**

Run: `sh test/verify_docs.sh`

Expected: `documentation checks passed`.

### Task 2: Rename the ROM Role Configuration

**Files:**
- Modify: `test/test_temperature_engine/test_config.cpp`
- Modify: `include/config.hpp`
- Modify: `src/ds18b20_reader.cpp`
- Modify: `README.md`
- Modify: `docs/hardware-installation.md`

**Interfaces:**
- Produces: `RuntimeConfig::main_tank_sensor` and
  `RuntimeConfig::sump_return_sensor` as the two fixed-ROM entries.
- Preserves: `TemperatureSample::display_c` / `return_c`, alarm semantics and
  telemetry JSON fields.

- [x] **Step 1: Write a failing configuration test**

Assert that both new role fields are unconfigured by default, then assign two
different ROM byte arrays and assert that both become configured.

- [x] **Step 2: Run the focused test and observe the expected failure**

Run the configuration-test command from `HANDOVER_SOFTWARE_20260716.md`.

Expected: compile failure because `main_tank_sensor` and
`sump_return_sensor` do not exist yet.

- [x] **Step 3: Rename only the ROM configuration boundary**

Replace `display_sensor` / `return_sensor` in `RuntimeConfig` and
`Ds18b20Reader` with the two approved hardware-role names. Do not rename
`TemperatureSample`, MQTT JSON or event fields.

- [x] **Step 4: Re-run the focused configuration test**

Expected: `firmware configuration tests passed`.

### Task 3: Revalidate the Existing S3 + Bark MVP

**Files:**
- Verify only: `include/config.hpp`, `src/main.cpp`, `src/bark_notifier.cpp`, `lib/temperature_engine/`, `lib/transport_contract/`.

**Interfaces:**
- Consumes: existing S3 build target and direct-Bark default configuration.
- Produces: fresh evidence that final hardware-documentation changes did not change alarm, queue, Bark-contract or firmware-build behavior.

- [x] **Step 1: Run all four C++ host test executables**

Run the commands recorded in `HANDOVER_SOFTWARE_20260716.md`, including `delivery_coordinator.cpp` in the transport-contract executable.

Expected: all four programs exit 0.

- [x] **Step 2: Run the Bark forwarder test and documentation check**

Run: `node --test cloud/bark-forwarder/index.test.mjs && sh test/verify_docs.sh`

Expected: two Node tests pass and documentation checks pass.

- [x] **Step 3: Build the final ESP32-S3 target**

Run: `/Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8`

Expected: a successful ESP32S3 build using 16 MB Flash and the 16 MB partition table.
