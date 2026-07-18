# Tencent Heartbeat Client and Live Cloud Completion Plan

> **For agentic workers:** Execute this plan inline in the current task. Sub-agent execution is not authorized. Use test-driven development for new security-sensitive behavior.

**Goal:** Complete every software and Tencent Cloud step that does not require the physical ESP32-S3 or DS18B20 probes.

**Architecture:** Direct Bark remains the temperature-alert primary path. A separately scheduled ESP32 HTTPS client posts HMAC-SHA256 heartbeats to the existing Shanghai SCF function. The function overwrites one COS state object, while a five-minute timer performs offline/recovery notification transitions.

**Tech Stack:** C++17 host tests, Arduino-ESP32, PlatformIO, mbedTLS, Node.js 20, Tencent SCF, COS, Bark.

## Global Constraints

- Do not change approved temperature thresholds, durations, hysteresis, reminders or sensor roles.
- Heartbeat failures must not block temperature sampling, Bark, or optional MQTT.
- Do not use `setInsecure()`, long-term Tencent API credentials, CLS, APM, VPC, filesystems, or pre-provisioned concurrency.
- Do not print, commit or persist real Bark keys, device secrets, signatures or cloud URLs in tracked files.
- Do not perform Git write operations from this repository.

---

### Task 1: Portable Heartbeat Contract and Scheduling

**Files:**
- Create: `lib/heartbeat_contract/include/heartbeat_contract.hpp`
- Create: `lib/heartbeat_contract/src/heartbeat_contract.cpp`
- Create: `test/test_heartbeat_contract/test_heartbeat_contract.cpp`
- Modify: `platformio.ini`

- [x] Write failing host tests for exact JSON/canonical construction, escaping, nonce format, nominal interval and retry backoff.
- [x] Run the focused test and record the expected failure.
- [x] Implement the minimum portable contract and scheduler behavior.
- [x] Re-run the focused test with `-Wall -Wextra -Werror`.

### Task 2: ESP32 HTTPS/HMAC Adapter

**Files:**
- Create: `src/heartbeat_notifier.hpp`
- Create: `src/heartbeat_notifier.cpp`
- Modify: `src/main.cpp`
- Modify: `include/config.hpp`
- Modify: `include/secrets.example.hpp`

- [x] Add configuration assertions for the default five-minute interval and independent enabled flag.
- [x] Implement UTC synchronization, random nonce generation, SHA-256/HMAC-SHA256 and verified-TLS POST.
- [x] Integrate latest-valid-sample heartbeats after Bark/MQTT processing with independent retry.
- [x] Ensure no secret, signature, URL or full payload is logged.

### Task 3: Signed Software Device Simulator

**Files:**
- Create: `cloud/tencent-scf/scripts/send-signed-heartbeat.mjs`
- Create: `cloud/tencent-scf/test/send_signed_heartbeat.test.mjs`
- Modify: `cloud/tencent-scf/package.json`

- [x] Write failing tests for deterministic request construction, required environment variables and redacted output.
- [x] Implement a simulator that reads URL and secret only from environment variables.
- [x] Add npm scripts without adding production dependencies.
- [x] Re-run focused and full cloud tests.

### Task 4: Documentation Alignment

**Files:**
- Modify: `README.md`
- Modify: `HANDOVER_SOFTWARE_20260716.md`
- Modify: `cloud/tencent-scf/README.md`
- Modify: `docs/superpowers/specs/2026-07-15-single-tank-temperature-monitor-design.md`
- Modify: `docs/superpowers/plans/2026-07-16-tencent-scf-safe-mode.md`
- Modify: `test/verify_docs.sh`

- [x] Make direct Bark primary and Tencent heartbeat additive everywhere.
- [x] Replace wildcard COS permissions with the deployed exact object.
- [x] Document secret handling, Function URL, Timer, cost bounds and hardware boundary.
- [x] Extend and run documentation checks.

### Task 5: Local Completion Gate

- [x] Run all portable C++ suites with warnings as errors.
- [x] Run all Node suites.
- [x] Run documentation checks and secret/insecure-TLS scans.
- [x] Build `waveshare_esp32s3_n16r8` with PlatformIO.
- [x] Inspect the firmware resource report and deployment-package manifest.

### Task 6: Tencent Cloud Live Configuration

- [x] Configure fixed bucket/region/device/offline variables.
- [x] Enter Bark key and a temporary simulator device secret without placing either in source.
- [x] Create a Function URL with no authentication layer beyond the application HMAC.
- [x] Create a five-minute Timer trigger.
- [x] Re-verify runtime role, single-object COS policy, memory, timeout, quota, logging and trigger settings.

### Task 7: Synthetic End-to-End Acceptance

- [x] Send a valid signed heartbeat and confirm HTTP success.
- [x] Confirm COS contains only the fixed `devices/tank01/state.json` state object.
- [x] Send invalid/stale/replayed requests and confirm rejection without state mutation.
- [x] Trigger healthy/no-op, first offline, repeated offline and recovery transitions.
- [x] Confirm Bark receives exactly one offline and one recovery notification.
- [ ] Rotate or record the final device secret for later firmware installation without placing it in tracked files.
- [x] Mark the older SCF plan’s deployment steps complete only after live evidence exists.

### Task 8: Hardware-Only Handoff

- [x] List only the remaining physical-board, probe, calibration and soak-test steps.
- [x] Do not claim end-to-end MVP completion until the real ESP32-S3 passes them.
