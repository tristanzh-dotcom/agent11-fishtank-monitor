# Single Tank Temperature Monitor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build flashable ESP32-C3 firmware and host-verifiable deterministic monitoring logic for one bottom-filter aquarium.

**Architecture:** A portable C++ `TemperatureEngine` owns threshold, duration, hysteresis, deduplication and event semantics. Arduino adapters own DS18B20 polling, Wi-Fi, TLS MQTT telemetry and optional Bark delivery. Host tests exercise the same engine through deterministic and randomized temperature traces.

**Tech Stack:** C++17, Arduino framework for ESP32-C3, PlatformIO, Unity host tests, OneWire, DallasTemperature, ArduinoJson, PubSubClient.

## Global Constraints

- Board: `esp32-c3-devkitm-1`; framework: Arduino.
- Samples occur every 30 seconds; no relay, heater, pump or mains-control code is permitted.
- `display` is the only animal-temperature authority; `return` is diagnostic only.
- Secrets live only in `include/secrets.hpp`, copied locally from `include/secrets.example.hpp`.
- TLS certificate validation is mandatory; never call `setInsecure()`.
- Git write operations are prohibited in this project workspace.

---

### Task 1: Create portable domain types and temperature engine

**Files:**
- Create: `lib/temperature_engine/include/temperature_engine.hpp`
- Create: `lib/temperature_engine/src/temperature_engine.cpp`
- Test: `test/test_temperature_engine/test_temperature_engine.cpp`

**Interfaces:**
- Produces `TemperatureEngine::ingest(const TemperatureSample&) -> std::vector<MonitorEvent>`.
- Produces `TemperaturePolicy`, `TemperatureSample`, `MonitorEvent`, `EventType`, `EventState`, `Severity`.

- [x] **Step 1: Write failing tests** for high alert duration, emergency escalation and recovery hysteresis.
- [x] **Step 2: Run** `g++ -std=c++17 -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests`; expected failure because engine files do not exist.
- [x] **Step 3: Write minimal types and engine** which emit opened, reminder and resolved events using elapsed milliseconds.
- [x] **Step 4: Re-run the same command**; expected success.

### Task 2: Add sensor fault, rapid-change and circulation diagnostics

**Files:**
- Modify: `lib/temperature_engine/include/temperature_engine.hpp`
- Modify: `lib/temperature_engine/src/temperature_engine.cpp`
- Modify: `test/test_temperature_engine/test_temperature_engine.cpp`

**Interfaces:**
- Consumes the Task 1 engine API.
- Adds `SensorHealth` and event types `sensor_fault`, `temperature_rapid_change`, `temperature_rapid_change_critical`, `temperature_gradient`.

- [x] **Step 1: Write failing tests** for two invalid display samples, one-degree change over thirty minutes and sustained 0.8°C display-return gradient.
- [x] **Step 2: Run the host test command**; expected assertion failures for unimplemented behavior.
- [x] **Step 3: Implement only the event paths required by those tests**, retaining display temperature as the animal authority.
- [x] **Step 4: Re-run the host test command**; expected success.

### Task 3: Add simulation regression suite

**Files:**
- Create: `test/test_temperature_engine/test_temperature_simulation.cpp`
- Modify: `test/test_temperature_engine/test_temperature_engine.cpp`

**Interfaces:**
- Consumes `TemperatureEngine::ingest`.
- Produces deterministic simulation evidence for summer, winter, heating failure, cold failure, water-change shock and noisy steady-state traces.

- [x] **Step 1: Write failing simulation assertions** for no alert during 26–27°C summer drift and exactly one open/reminder/resolved incident per sustained high-temperature episode.
- [x] **Step 2: Run the host test command** with both test files; expected assertion failure.
- [x] **Step 3: Implement the smallest policy/default adjustment** needed to satisfy approved thresholds.
- [x] **Step 4: Run the host test command** and a seeded 10,000-sample noise loop; expected success.

### Task 4: Add PlatformIO configuration and firmware configuration boundary

**Files:**
- Create: `platformio.ini`
- Create: `include/secrets.example.hpp`
- Create: `include/config.hpp`
- Create: `.gitignore`
- Test: PlatformIO build for `esp32c3`.

**Interfaces:**
- Produces `RuntimeConfig` containing sensor ROM addresses, Wi-Fi, Aliyun and optional Bark settings.
- Firmware consumes `TemperatureEngine` without Arduino dependencies.

- [x] **Step 1: Write a host compilation test** proving `config.hpp` can instantiate the approved default policy.
- [x] **Step 2: Run the host test command**; expected failure because config does not exist.
- [x] **Step 3: Add the PlatformIO board config, secret template and compile-time config validation.**
- [x] **Step 4: Run host tests and** `pio run -e esp32c3`; expected successful host tests and firmware compilation.

### Task 5: Implement DS18B20 acquisition, MQTT telemetry and optional Bark adapter

**Files:**
- Create: `src/main.cpp`
- Create: `src/ds18b20_reader.hpp`
- Create: `src/ds18b20_reader.cpp`
- Create: `src/aliyun_mqtt.hpp`
- Create: `src/aliyun_mqtt.cpp`
- Create: `src/bark_notifier.hpp`
- Create: `src/bark_notifier.cpp`
- Test: `test/test_transport_contract/test_transport_contract.cpp`

**Interfaces:**
- `Ds18b20Reader::read()` returns a timestamped `TemperatureSample`.
- `AliyunMqtt::publishTelemetry()` publishes JSON to `/${productKey}/${deviceName}/user/aquarium/telemetry`.
- `AliyunMqtt::publishEvent()` publishes JSON to `/${productKey}/${deviceName}/user/aquarium/events`.
- `BarkNotifier::notify()` sends only a redacted title/body/group/level/id payload when explicitly enabled.

- [x] **Step 1: Write failing payload-contract tests** for JSON fields, absent secret fields and redacted Bark content.
- [x] **Step 2: Run host tests**; expected failures because adapters/payload functions are absent.
- [x] **Step 3: Implement adapters with certificate validation, bounded timeouts and non-blocking delivery failure handling.**
- [x] **Step 4: Run host tests and** `pio run -e esp32c3`; expected successful output.

### Task 6: Add operating instructions and perform final verification

**Files:**
- Create: `README.md`
- Create: `docs/hardware-installation.md`
- Modify: `docs/superpowers/specs/2026-07-15-single-tank-temperature-monitor-design.md`

**Interfaces:**
- Documents first flash, ROM discovery, secret provisioning, 48–72 hour soak test, installed probe positions, PlatformIO flash command and Aliyun/Bark cutover.

- [x] **Step 1: Write documentation checks** as a shell script that verifies required headings and that no literal secret template value is present in firmware sources.
- [ ] **Step 2: Run the documentation check**; expected failure because documents do not exist. (Superseded: documentation was created in the same implementation slice.)
- [x] **Step 3: Write exact hardware and flashing instructions.**
- [x] **Step 4: Run all host tests, documentation check and** `pio run -e esp32c3`; expected success.

### Task 7: Complete non-hardware transport reliability and cloud forwarding

**Files:**
- Create: `lib/transport_contract/*`, `test/test_transport_contract/*`, `cloud/bark-forwarder/*`
- Modify: `src/aliyun_mqtt.*`, `src/bark_notifier.cpp`, `src/ds18b20_reader.cpp`, `src/main.cpp`, `README.md`

- [x] Write failing host tests for event/telemetry JSON, Bark message redaction,
  outbox behavior, and a Function Compute Bark request.
- [x] Implement portable contracts, bounded retry/backoff, and RAM event outbox.
- [x] Add ROM-address serial discovery output and cloud forwarder source without
  secrets or external deployment.
- [x] Run host tests, Node tests, documentation check, and ESP32-C3 build.
