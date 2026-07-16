# Direct Bark Primary Transport Implementation Plan

> **For agentic workers:** Execute this plan inline in the current task. Sub-agent execution is not authorized for this task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make direct HTTPS Bark delivery the default ESP32-S3 MVP path while keeping MQTT optional and unable to block Bark.

**Architecture:** Keep the deterministic temperature engine unchanged. Add a portable coordinator with independent 16-event Bark and MQTT queues, make Bark JSON POST delivery acknowledge only the Bark queue, and gate all MQTT work behind a default-off runtime flag.

**Tech Stack:** C++17 host tests, Arduino-ESP32, PlatformIO Espressif32, HTTPClient/WiFiClientSecure, existing transport contracts.

## Global Constraints

- Target remains Waveshare `ESP32-S3-DEV-KIT-N16R8-M` with PlatformIO environment `waveshare_esp32s3_n16r8`.
- Preserve every approved alarm threshold, duration, severity, hysteresis and reminder rule.
- Default transport mode is direct Bark enabled and MQTT disabled.
- MQTT failure or absence must not block sampling, alarm evaluation or Bark delivery.
- Bark Device Key must not appear in source defaults, URL, logs, event text or test output.
- Do not call external services, use real credentials, deploy cloud resources or add mains-control behavior.
- This directory is not a Git repository; do not perform Git write operations.

---

### Task 1: Lock transport defaults and independent queue behavior

**Files:**
- Modify: `test/test_temperature_engine/test_config.cpp`
- Modify: `test/test_transport_contract/test_transport_contract.cpp`
- Create: `lib/transport_contract/include/delivery_coordinator.hpp`
- Create: `lib/transport_contract/src/delivery_coordinator.cpp`
- Modify: `include/config.hpp`

**Interfaces:**
- Produces `RuntimeConfig::bark_enabled == true` and `RuntimeConfig::mqtt_enabled == false`.
- Produces `DeliveryCoordinator::enqueue`, `bark_front`, `mqtt_front`, `acknowledge_bark`, and `acknowledge_mqtt`.

- [x] Add assertions for the default flags and independent Bark/MQTT acknowledgements.
- [x] Run the two focused host tests and confirm compile/assertion failure for missing behavior.
- [x] Implement the minimal config fields and coordinator using two `EventOutbox` instances.
- [x] Re-run both focused tests and confirm they pass.

### Task 2: Lock and implement Bark HTTPS JSON POST

**Files:**
- Modify: `test/test_transport_contract/test_transport_contract.cpp`
- Modify: `lib/transport_contract/include/transport_contract.hpp`
- Modify: `lib/transport_contract/src/transport_contract.cpp`
- Modify: `src/bark_notifier.cpp`

**Interfaces:**
- Produces `bark_request_json(const BarkMessage&, const std::string&) -> std::string`.
- `BarkNotifier::notify` posts that JSON to fixed URL `https://api.day.app/push` and succeeds only for HTTP 2xx.

- [x] Add an exact JSON request assertion plus quote/backslash escaping assertion.
- [x] Run the focused transport test and confirm it fails because the builder is absent.
- [x] Implement JSON escaping and request generation, then switch the Arduino adapter from key-in-URL GET to JSON POST.
- [x] Re-run the focused transport test and confirm it passes.

### Task 3: Make firmware orchestration Bark-first and MQTT optional

**Files:**
- Modify: `src/main.cpp`
- Modify: `include/secrets.example.hpp`
- Modify: `platformio.ini`

**Interfaces:**
- Consumes `DeliveryCoordinator` and `RuntimeConfig` transport flags.
- Produces independent Bark and optional MQTT flushing; MQTT telemetry is skipped when disabled.

- [x] Wire generated events into both coordinator queues according to the runtime flags.
- [x] Acknowledge each queue only after its own transport succeeds.
- [x] Remove the obsolete secret-level Bark enable flag and clarify placeholder comments.
- [x] Add the coordinator source to host/build integration without changing dependency versions.

### Task 4: Align authoritative handoff and user documentation

**Files:**
- Modify: `HANDOVER_SOFTWARE_20260716.md`
- Modify: `README.md`
- Modify: `docs/hardware-installation.md`
- Modify: `test/verify_docs.sh`

**Interfaces:**
- Documents S3 → Bark as the MVP path, MQTT as optional, and no external cloud prerequisite.

- [x] Extend documentation checks for Bark-primary language and rejection of current Aliyun-required guidance.
- [x] Run the documentation check and confirm it fails against stale text.
- [x] Update current instructions, configuration order and soak-test expectations.
- [x] Re-run documentation checks and confirm they pass.

### Task 5: Full regression and S3 build

**Files:**
- Verify only: all production, test and documentation files.

**Interfaces:**
- Produces executable evidence without real network calls or hardware claims.

- [x] Run both temperature-engine host suites, config tests and transport-contract tests with `-Wall -Wextra -Werror`.
- [x] Run the legacy cloud-forwarder Node tests as retained optional-path regression.
- [x] Run documentation checks and scan for `setInsecure`, literal secrets and key-in-URL construction.
- [x] Build `waveshare_esp32s3_n16r8` with PlatformIO and inspect the image target/resource report.
- [x] Re-read this plan and report any hardware, ROM, certificate or key-dependent acceptance item not executed.
