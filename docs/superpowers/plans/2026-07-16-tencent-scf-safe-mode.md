# Tencent SCF Safe-Mode Heartbeat Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Deploy a cost-bounded Tencent Cloud heartbeat receiver and offline checker for `tank01` without enabling CLS.

**Architecture:** One serialized SCF event function owns both the Function URL and timer triggers. It authenticates compact ESP32 heartbeats with HMAC-SHA256, enforces a 1 KiB body limit and a 500 ms response floor, and overwrites one fixed COS state object. Timer events read the same state and send Bark on offline/recovery transitions. A 128 MB maximum exclusive quota on the single 128 MB function plus in-process serialization prevents timer/heartbeat read-modify-write races.

**Tech Stack:** Node.js 20 ESM, built-in `node:test`, `node:crypto`, `fetch`, `cos-nodejs-sdk-v5`, Tencent SCF event functions, COS.

## Global Constraints

- Region is `ap-shanghai` for COS, SCF, and all future cloud resources.
- COS bucket is `fishtank-monitor-1454792551`, private, single-AZ, SSE-COS.
- Do not open CLS and do not enable SCF log delivery.
- Exactly one function handles both triggers; memory is 128 MB, pre-provisioned concurrency is zero, and maximum exclusive quota is 128 MB.
- Heartbeat request bodies are at most 1024 bytes and every public request takes at least 500 ms.
- Device credentials and Bark keys are environment variables only and must never appear in source, tests, console output, URLs, or logs.
- Do not perform Git write operations from this repository.

---

### Task 1: Signed Heartbeat Contract

**Files:**
- Create: `cloud/tencent-scf/src/heartbeat_contract.mjs`
- Create: `cloud/tencent-scf/test/heartbeat_contract.test.mjs`

**Interfaces:**
- Produces: `authenticateHeartbeat(event, options)` and typed `RequestError`.
- `authenticateHeartbeat` returns `{ payload, rawBody }`; normalized payload contains `{ deviceId, sentAtMs, nonce, mainC, sumpC, uptimeMs }`.

- [x] **Step 1: Write failing tests** for body-size rejection, malformed JSON, unknown device, stale timestamp, invalid nonce, invalid signature, and a valid signed request.
- [x] **Step 2: Run** `node --test cloud/tencent-scf/test/heartbeat_contract.test.mjs` and verify failure because the module does not exist.
- [x] **Step 3: Implement minimal validation** using `crypto.createHash`, `crypto.createHmac`, and `crypto.timingSafeEqual` with canonical input `v1\n<timestamp>\n<nonce>\n<sha256(rawBody)>`.
- [x] **Step 4: Re-run** the focused test and verify all cases pass.

### Task 2: Fixed-Object COS Heartbeat Handler

**Files:**
- Create: `cloud/tencent-scf/src/cos_state_store.mjs`
- Create: `cloud/tencent-scf/src/heartbeat_handler.mjs`
- Create: `cloud/tencent-scf/monitor.js`
- Create: `cloud/tencent-scf/serializer.js`
- Create: `cloud/tencent-scf/test/heartbeat_handler.test.mjs`
- Create: `cloud/tencent-scf/test/cos_state_store.test.mjs`

**Interfaces:**
- Consumes: `authenticateHeartbeat` from Task 1.
- Produces: `createHeartbeatHandler({ store, deviceSecrets, clock, sleep })` and SCF export `main_handler(event, context)`.
- Store contract: `getDeviceState(deviceId)` and `saveDeviceState(deviceId, state)`; COS key is exactly `devices/<deviceId>/state.json`.

- [x] **Step 1: Write failing tests** proving invalid requests never touch COS, valid requests overwrite a fixed key, nonce replay is rejected, recovery is recorded, response payload is redacted, and elapsed time is at least 500 ms.
- [x] **Step 2: Run** `node --test cloud/tencent-scf/test/heartbeat_handler.test.mjs` and verify the expected missing-module failure.
- [x] **Step 3: Implement the handler** with dependency injection, one fixed state document, no console logging, and explicit JSON HTTP responses.
- [x] **Step 4: Implement the production COS adapter** with temporary SCF role credentials from `TENCENTCLOUD_SECRETID`, `TENCENTCLOUD_SECRETKEY`, and `TENCENTCLOUD_SESSIONTOKEN`.
- [x] **Step 5: Re-run** the focused tests and verify they pass.

### Task 3: Offline/Recovery State Machine and Deployment Package

**Files:**
- Create: `cloud/tencent-scf/src/offline_checker.mjs`
- Create: `cloud/tencent-scf/src/bark_notifier.mjs`
- Consume timer events through `cloud/tencent-scf/monitor.js`
- Create: `cloud/tencent-scf/test/offline_checker.test.mjs`
- Create: `cloud/tencent-scf/package.json`
- Create: `cloud/tencent-scf/README.md`

**Interfaces:**
- Consumes: COS store contract from Task 2.
- Produces: `evaluateOfflineTransition(state, nowMs, offlineAfterMs)` and `createOfflineChecker({ store, notifier, deviceIds, clock })`.
- Bark contract: `notifier.send(alert): Promise<void>`; the key is read only from `BARK_KEY`.

- [x] **Step 1: Write failing tests** for healthy/no-op, first offline transition, repeated offline/no duplicate Bark, recovery transition, missing state, and Bark failure preserving the prior notification state.
- [x] **Step 2: Run** `node --test cloud/tencent-scf/test/offline_checker.test.mjs` and verify the expected missing-module failure.
- [x] **Step 3: Implement the transition function and checker** with a 15-minute default offline threshold and a single `tank01` device list.
- [x] **Step 4: Add package metadata and deployment documentation** describing Node.js 20, handler names, environment variables, disabled CLS, 128 MB memory, 3-second timeout, no pre-provisioned concurrency, and 128 MB maximum exclusive quota.
- [x] **Step 5: Run** the full tests and package dry-run; verify zero failures and that no local test or secret files enter the package.

### Task 4: Cloud Deployment Checkpoint

**Files:**
- Create generated artifact only after verification: `.build/tencent-scf-safe-mode.zip`

**Interfaces:**
- Consumes the tested package from Tasks 1-3.
- Produces one inactive Shanghai SCF function ready for private environment variables and two triggers.

- [x] **Step 1: Build the ZIP** from the verified package without repository secrets or test fixtures.
- [x] **Step 2: Inspect the ZIP manifest** and verify only runtime files and production dependencies are present.
- [x] **Step 3: Obtain TZ confirmation immediately before uploading the ZIP to Tencent Cloud.**
- [x] **Step 4: Create one `fishtank-monitor` function** in Shanghai with log delivery disabled and no triggers initially.
- [x] **Step 5: Configure least-privilege COS access, 128 MB memory, ten-second timeout, zero pre-provisioned concurrency, and 128 MB maximum exclusive quota.**
- [x] **Step 6: Stop before entering Bark/device secrets or enabling public/timer triggers; those require a separate credential checkpoint and live verification.**

### Task 5: Live Activation and Synthetic Acceptance

- [x] Enter the private Bark key and temporary simulator device secret as masked environment variables.
- [x] Enable the public Function URL with application-layer HMAC authentication.
- [x] Enable the five-minute `fishtank-monitor-offline-check` Timer.
- [x] Verify valid, replayed, invalid-signature and stale signed requests against the live function.
- [x] Verify the single fixed COS state object and absence of invalid nonce mutations.
- [x] Verify first offline, repeated offline no-op, heartbeat recovery, first recovery and repeated recovery no-op.
- [x] Verify Bark accepts exactly one offline and one recovery transition before COS state is committed.
- [ ] Rotate the final device secret immediately before real firmware installation.
