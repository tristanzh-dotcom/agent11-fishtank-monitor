# Remote Five-Tank Temperature Shortcut Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add an optional, authenticated five-tank temperature snapshot to the existing ESP32 heartbeat and expose it through a private SCF text endpoint without changing the existing `/state`, Bark, offline, or Tab5 behavior.

**Architecture:** The firmware serializes an optional `temperature_snapshot` beside the existing heartbeat fields and omits it when its UTF-8 or full-body limits are exceeded. The SCF validates and stores the optional snapshot in the same latest COS object, while the existing state handler serves either the unchanged JSON projection or the new exact text path with server-side freshness and offline checks.

**Tech Stack:** C++17/PlatformIO firmware contracts, Node.js 20 native test runner, Tencent COS state store.

**Spec:** `docs/superpowers/specs/2026-09-12-remote-five-temperature-shortcut-design.md`

## Global Constraints

- Keep `FishTankStateV1`, existing `/api/v1/devices/tank01/state`, Bark alerts, scheduled reports, and Tab5 behavior unchanged.
- Keep the existing HMAC body-signature input and the 2,048-byte decoded heartbeat limit.
- `temperature_snapshot.summary_text` is non-empty, well-formed UTF-8, and at most 768 UTF-8 bytes; invalid or oversized optional data is omitted by firmware and rejected by SCF when supplied.
- A missing snapshot in a newer heartbeat clears any previous snapshot; offline transitions preserve it.
- The summary endpoint uses the existing Bearer token, returns `text/plain; charset=utf-8` with `Cache-Control: no-store`, and never writes COS or sends Bark.
- Do not deploy, flash, or perform physical/mobile acceptance in this implementation pass.

---

### Task 1: Extend the firmware heartbeat contract

**Files:**
- Modify: `lib/heartbeat_contract/include/heartbeat_contract.hpp`
- Modify: `lib/heartbeat_contract/src/heartbeat_contract.cpp`
- Test: `test/test_heartbeat_contract/test_heartbeat_contract.cpp`

**Interfaces:**
- Add `TemperatureSnapshot { sampled_at_ms, summary_text }` and an optional member on `HeartbeatPayload`.
- Keep `heartbeat_json` and `signature_canonical` as the public serialization/signature entry points.

- [x] Add host assertions for valid snapshot serialization, escaped UTF-8 text, invalid/empty snapshot omission, the 768-byte boundary, full-body 2,048-byte omission, and changed signature body input.
- [x] Run the focused C++ contract command and observe the new assertions fail before implementation.
- [x] Implement minimal validation and two-pass serialization so only the optional snapshot is omitted when it violates its own or the full-body limit.
- [x] Re-run the focused contract test and retain the existing heartbeat field output exactly when no snapshot is present.

### Task 2: Connect the snapshot to the ESP32 heartbeat

**Files:**
- Modify: `src/heartbeat_notifier.hpp`
- Modify: `src/heartbeat_notifier.cpp`
- Modify: `src/main.cpp`

**Interfaces:**
- Extend `HeartbeatNotifier::notify` with an optional `heartbeat::TemperatureSnapshot` argument.
- Build the snapshot only at heartbeat time from the existing `daily_summary_body` formatter and the current five-probe readings.

- [x] Add/adjust the host contract fixture needed to compile the new optional argument without changing existing callers.
- [x] Run the affected host contract/build command before implementation and confirm the new interface is absent.
- [x] Implement the optional argument and pass it through the existing HMAC POST path.
- [x] Refactor the existing daily snapshot helper only enough to share the five-tank summary formatting and the same valid Unix sample time; do not alter sampling, scheduling, or alert behavior.
- [x] Build the target PlatformIO environment without uploading hardware.

### Task 3: Validate and persist the optional snapshot in SCF

**Files:**
- Modify: `cloud/tencent-scf/src/heartbeat_contract.mjs`
- Modify: `cloud/tencent-scf/src/heartbeat_handler.mjs`
- Modify: `cloud/tencent-scf/src/offline_checker.mjs` only if a focused regression proves preservation needs it.
- Test: `cloud/tencent-scf/test/heartbeat_contract.test.mjs`
- Test: `cloud/tencent-scf/test/heartbeat_handler.test.mjs`

**Interfaces:**
- Normalize the wire field to `{ sampledAtMs, summaryText }` and attach it only when present.
- Construct the latest state without carrying a previous snapshot when the incoming heartbeat omits it.

- [x] Add failing tests for legal/illegal snapshot fields, UTF-8 byte limits, old-heartbeat clearing, and offline preservation.
- [x] Run the focused Node tests and confirm the expected failures.
- [x] Implement validation and state projection while preserving all existing heartbeat and replay behavior.
- [x] Run the heartbeat contract and handler tests again.

### Task 4: Add the private text summary route

**Files:**
- Modify: `cloud/tencent-scf/src/state_read_handler.mjs`
- Modify: `cloud/tencent-scf/src/scf_runtime.mjs`
- Test: `cloud/tencent-scf/test/state_read_handler.test.mjs`
- Test: `cloud/tencent-scf/test/scf_runtime.test.mjs`

**Interfaces:**
- Keep the existing state route in `createStateReadHandler` and add the exact `/api/v1/devices/tank01/temperature-summary` GET path.
- Inject a clock for deterministic age checks.

- [x] Add failing tests for token/method/path handling, no snapshot, offline precedence, future timestamps, exactly 10 minutes, stale timestamps, plain-text headers, and no writes.
- [x] Run the focused state tests and confirm the expected failures.
- [x] Implement the route with the existing constant-time Bearer check, `200` text responses for data/unavailable messages, Chinese text `401/405/503` errors, and unchanged JSON `/state` behavior.
- [x] Route POST requests on the exact summary path to the read handler so they cannot enter heartbeat storage.
- [x] Run the state, runtime, and full SCF test suite.

### Task 5: Documentation and final verification

**Files:**
- Modify: `cloud/tencent-scf/README.md`

- [x] Document the new private GET path and explain that it is backward-compatible with old heartbeats while retaining the existing deployment confirmation boundary.
- [x] Run `git diff --check`.
- [x] Run the complete focused host tests, SCF tests, and both PlatformIO target builds; read each exit status and output before reporting results.
- [x] Inspect the final diff and confirm no credentials, deployment actions, or uploads were introduced; the default Tab5 environment remains intact and the remote candidate explicitly excludes it.
