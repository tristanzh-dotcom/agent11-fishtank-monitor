# Heartbeat State Snapshot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Publish truthful latest-state snapshots from the ESP32 through Tencent SCF and expose the approved read-only `FishTankStateV1` API without requiring a connected development board.

**Architecture:** Add a deterministic ESP32 `ActiveEventSnapshot` projection that consumes the existing `TemperatureEngine` event stream. Extend the HMAC heartbeat with nullable probe readings and that complete projection; SCF validates and overwrites the existing private COS object. A method/path router adds a separate Bearer-protected GET projection without exposing COS internals or credentials.

**Tech Stack:** C++17 host `assert` tests, Arduino/ESP32-S3 PlatformIO, Node.js 20 ESM, `node:test`, built-in `node:crypto`, Tencent SCF, COS.

## Global Constraints

- Preserve approved thresholds, durations, hysteresis, probe roles, Bark primary delivery, MQTT default-off behavior, and no-220V-control boundary.
- Keep `FishTankStateV1` public fields `display_c` and `return_c`; serial `main_tank` and `sump_tank` are human-readable labels only.
- Only ESP32 owns temperature-event decisions; SCF and Agent12 must never infer events from readings.
- Heartbeats are signed HMAC-SHA256 POST bodies of at most 2,048 UTF-8 bytes; public GET uses a separate `STATE_READ_TOKEN` with constant-time comparison.
- Missing or untrustworthy readings are JSON `null`, never zero or stale fabricated values.
- COS remains private and only `devices/tank01/state.json` may be read/written.
- Do not deploy SCF, enter credentials, rotate secrets, or connect the development board in this plan.
- Do not run Git write operations in this repository.

---

### Task 1: Add ESP32 active-event projection

**Files:**
- Create: `lib/active_event_snapshot/include/active_event_snapshot.hpp`
- Create: `lib/active_event_snapshot/src/active_event_snapshot.cpp`
- Create: `test/test_active_event_snapshot/test_active_event_snapshot.cpp`

**Interfaces:**
- Consumes: `std::vector<TemperatureEvent>` from `TemperatureEngine::ingest`.
- Produces: `ActiveEvent { EventType type; EventState state; Severity severity; uint64_t at_ms; optional<double> display_c; }` and `ActiveEventSnapshot::events()`.

- [ ] **Step 1: Write failing host tests** for one `opened` event, an `escalated` replacement, a `reminder` replacement, a `resolved` removal, and `sensor_fault` forcing `display_c == nullopt`.
- [ ] **Step 2: Compile and run the new test** before implementation.

  Run:

  ```sh
  g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/active_event_snapshot/include test/test_active_event_snapshot/test_active_event_snapshot.cpp lib/active_event_snapshot/src/active_event_snapshot.cpp -o .build/active_event_snapshot_tests && ./.build/active_event_snapshot_tests
  ```

  Expected: compile failure because `active_event_snapshot.hpp` does not yet exist.

- [ ] **Step 3: Implement the minimum projection.** Keep at most one event per `EventType`; replace on `opened`, `escalated`, and `reminder`; erase on `resolved`; preserve deterministic enum order; force `sensor_fault` to `nullopt`.
- [ ] **Step 4: Re-run the same command** and require `active event snapshot tests passed`.

### Task 2: Extend the C++ signed heartbeat contract

**Files:**
- Modify: `lib/heartbeat_contract/include/heartbeat_contract.hpp`
- Modify: `lib/heartbeat_contract/src/heartbeat_contract.cpp`
- Modify: `test/test_heartbeat_contract/test_heartbeat_contract.cpp`

**Interfaces:**
- Consumes: `HeartbeatPayload { device_id, sent_at_ms, nonce, optional<double> main_c, optional<double> sump_c, uptime_ms, vector<ActiveEvent> active_events }`.
- Produces: exact JSON body with nullable readings and `active_events`, plus unchanged `v1` canonical signature string.

- [ ] **Step 1: Write failing assertions** for JSON `null`, one N3 event serialized as lower-case `n3`, a fault event serialized with `display_c:null`, deterministic field order, and a worst-case seven-event body shorter than or equal to 2,048 bytes.
- [ ] **Step 2: Compile/run the focused heartbeat test** and verify it fails because the new fields and JSON output are absent.
- [ ] **Step 3: Implement JSON serialization helpers** for nullable numbers, event type/state/severity names, and the fixed event array. Keep JSON escaping and signature canonicalization unchanged.
- [ ] **Step 4: Re-run the focused heartbeat test** and require `heartbeat contract tests passed`.

### Task 3: Connect the projection to firmware scheduling

**Files:**
- Modify: `src/heartbeat_notifier.hpp`
- Modify: `src/heartbeat_notifier.cpp`
- Modify: `src/main.cpp`
- Modify: `platformio.ini` only if the new local library is not discovered automatically

**Interfaces:**
- Consumes: the latest `TemperatureSample` and `ActiveEventSnapshot`.
- Produces: one signed heartbeat attempt per scheduler decision even when either reading is missing.

- [ ] **Step 1: Add a failing compile-time contract assertion** to the heartbeat host test for `HeartbeatNotifier::notify` consuming nullable readings and active events through the public heartbeat payload.
- [ ] **Step 2: Implement only the notifier/signature change** so the notifier creates the expanded `HeartbeatPayload` and preserves current NTP, CA, HMAC, timeouts, and HTTP-success behavior.
- [ ] **Step 3: Modify `main.cpp`** so it applies `engine.ingest(sample)` to the snapshot before delivery queues, removes the dual-validity gate, and passes the sample plus snapshot to heartbeat delivery.
- [ ] **Step 4: Run PlatformIO build**:

  ```sh
  /Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8
  ```

  Expected: successful compilation only; no claim about a real board.

### Task 4: Validate and persist v2 heartbeat state in SCF

**Files:**
- Modify: `cloud/tencent-scf/src/heartbeat_contract.mjs`
- Modify: `cloud/tencent-scf/src/heartbeat_handler.mjs`
- Modify: `cloud/tencent-scf/test/heartbeat_contract.test.mjs`
- Modify: `cloud/tencent-scf/test/heartbeat_handler.test.mjs`

**Interfaces:**
- Consumes: signed 2 KiB-max POST body carrying nullable `main_c`, `sump_c`, and `active_events`.
- Produces: internal COS `schemaVersion: 2` state with `activeEvents` and existing nonce/online/recovery fields.

- [ ] **Step 1: Write failing Node tests** that accept valid nullable readings and a valid active event; reject more than seven events, duplicate event types, invalid enum/state/severity, non-null `sensor_fault`, malformed nullable temperatures, and bodies above 2,048 bytes.
- [ ] **Step 2: Run** `node --test cloud/tencent-scf/test/heartbeat_contract.test.mjs cloud/tencent-scf/test/heartbeat_handler.test.mjs` and verify the failures are caused by missing v2 parsing/persistence.
- [ ] **Step 3: Implement strict normalizers** for nullable temperature and active events, increase `DEFAULT_MAX_BODY_BYTES` to 2,048, and make `nextState` write `schemaVersion: 2`, `mainC`, `sumpC`, and `activeEvents`.
- [ ] **Step 4: Re-run focused Node tests** and require all pass.

### Task 5: Add the read-only SCF state API

**Files:**
- Create: `cloud/tencent-scf/src/state_read_handler.mjs`
- Modify: `cloud/tencent-scf/src/runtime_config.mjs`
- Modify: `cloud/tencent-scf/src/scf_runtime.mjs`
- Create: `cloud/tencent-scf/test/state_read_handler.test.mjs`
- Modify: `cloud/tencent-scf/test/scf_runtime.test.mjs`
- Modify: `cloud/tencent-scf/test/entrypoints.test.mjs`

**Interfaces:**
- Consumes: `GET /api/v1/devices/tank01/state`, `Authorization: Bearer <STATE_READ_TOKEN>`, and v2 state from `getDeviceState`.
- Produces: `FishTankStateV1` or stable redacted `401/404/405/503` JSON errors.

- [ ] **Step 1: Write failing tests** for valid token projection, missing/wrong token without COS access, v1 state yielding `503 state_unavailable`, missing state yielding `404 state_not_found`, wrong method yielding `405 method_not_allowed`, and no `saveDeviceState` calls.
- [ ] **Step 2: Run** `node --test cloud/tencent-scf/test/state_read_handler.test.mjs cloud/tencent-scf/test/scf_runtime.test.mjs` and verify the expected missing-module/routing failure.
- [ ] **Step 3: Implement `state_read_handler.mjs`** with request-path validation, `timingSafeEqual` token comparison, safe v2-state projection, and redacted errors. Add `readStateApiConfig` to validate `STATE_READ_TOKEN` as 32–256 characters. Route only the exact GET path in `scf_runtime.mjs`; preserve POST heartbeat and Timer behavior.
- [ ] **Step 4: Re-run focused Node tests** and require all pass.

### Task 6: Documentation, package, and no-hardware completion gate

**Files:**
- Modify: `cloud/tencent-scf/README.md`
- Modify: `HANDOVER_SOFTWARE_CLOUD_STATUS_20260724.md`
- Modify: `test/verify_docs.sh` only for new deterministic documentation assertions
- Create: `.build/tencent-scf-state-api.zip` generated artifact only after verification

**Interfaces:**
- Consumes: final local SCF source and existing package script.
- Produces: redacted deployment instructions and a runtime-only ZIP; neither activates cloud state nor contains credentials/tests.

- [ ] **Step 1: Document** the 2 KiB signed heartbeat cap, v2 COS state, `STATE_READ_TOKEN` handling, GET route, and the explicit “deployment/credential/board still required” boundary.
- [ ] **Step 2: Run full local regression**:

  ```sh
  g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests
  g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_simulation.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_simulation_tests && ./.build/temperature_simulation_tests
  g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include test/test_temperature_engine/test_config.cpp -o .build/config_tests && ./.build/config_tests
  g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && ./.build/transport_contract_tests
  g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/active_event_snapshot/include test/test_active_event_snapshot/test_active_event_snapshot.cpp lib/active_event_snapshot/src/active_event_snapshot.cpp -o .build/active_event_snapshot_tests && ./.build/active_event_snapshot_tests
  g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/active_event_snapshot/include -Ilib/heartbeat_contract/include test/test_heartbeat_contract/test_heartbeat_contract.cpp lib/active_event_snapshot/src/active_event_snapshot.cpp lib/heartbeat_contract/src/heartbeat_contract.cpp -o .build/heartbeat_contract_tests && ./.build/heartbeat_contract_tests
  node --test cloud/bark-forwarder/index.test.mjs
  (cd cloud/tencent-scf && npm test && npm audit --audit-level=high)
  sh test/verify_docs.sh
  /Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8
  ```

  Expected: every command exits zero.

- [ ] **Step 3: Build and inspect the deployment ZIP** with `cloud/tencent-scf/scripts/build-package.sh .build/tencent-scf-state-api.zip`; ensure it contains production runtime files only and no `test/`, `secrets.hpp`, or local credentials.
- [ ] **Step 4: Stop and notify TZ** that deployment, private environment variables, firmware flashing, Wi-Fi/NTP, real heartbeat, Bark, and Agent12 live verification require the reconnected board and a separate Tencent Cloud confirmation.
