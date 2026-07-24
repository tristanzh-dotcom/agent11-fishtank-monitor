# Temperature Event Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement approved recovery, escalation, reminder, and invalid-sample semantics for rapid-change, gradient, and sensor-fault temperature events.

**Architecture:** `TemperatureEngine` remains the deterministic state owner. It will track one rapid-change incident with peak severity and an `escalated` state; the transport contract serializes that state without changing Bark delivery or cloud heartbeats. Invalid main-probe samples reset continuity timing and suppress temperature-event reminders while allowing the sensor-fault incident to notify.

**Tech Stack:** C++17, portable host `assert` tests, Arduino/ESP32-S3 PlatformIO build, existing transport contract.

## Global Constraints

- Preserve approved high/low thresholds, durations, hysteresis, 30-second sampling, probe roles, HTTPS paths, and no-mains-control boundary.
- Rapid-change: open at `>=1.0°C`, escalate once at `>=2.0°C`, and recover after 30 consecutive minutes of `<1.0°C` window movement.
- Gradient: open at `>0.8°C` for 10 minutes and recover only at `<=0.6°C` for 10 minutes.
- Two invalid main-probe samples open `sensor_fault`; invalid samples reset continuity timing, suppress other temperature-event reminders, and do not resolve temperature events.
- All open N2/N3 events remind at most once per 60 minutes.
- Do not run Git write commands in this repository.

---

### Task 1: Specify escalation in the portable API and transport contract

**Files:**
- Modify: `lib/temperature_engine/include/temperature_engine.hpp:10-62`
- Modify: `lib/transport_contract/src/transport_contract.cpp:54-125`
- Modify: `test/test_transport_contract/test_transport_contract.cpp:26-50`

**Interfaces:**
- Consumes: `TemperatureEvent { type, state, severity, at_ms, display_c }`.
- Produces: `EventState::escalated`, serialized as JSON/Bark state `"escalated"`.
- Produces: one rapid-change event type whose N2/N3 level is carried by `Severity`.

- [ ] **Step 1: Write the failing transport test**

```cpp
const TemperatureEvent escalation{EventType::temperature_rapid_change,
                                  EventState::escalated, Severity::n3,
                                  1800000, 28.0};
const auto payload = aquarium::transport::event_json(escalation);
assert(payload.find("\"event_type\":\"temperature_rapid_change\"") !=
       std::string::npos);
assert(payload.find("\"state\":\"escalated\"") != std::string::npos);
assert(payload.find("\"severity\":\"N3\"") != std::string::npos);
```

- [ ] **Step 2: Run the focused transport test and verify it fails**

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && ./.build/transport_contract_tests
```

Expected: compilation failure naming `EventState::escalated`.

- [ ] **Step 3: Add the smallest contract change**

```cpp
enum class EventState { opened, escalated, reminder, resolved };
```

Add `escalated -> "escalated"` to `event_state_name`. Preserve the JSON fields and derive the event ID from type, state, and timestamp as today.

- [ ] **Step 4: Re-run the focused transport test**

Expected: `transport contract tests passed`.

### Task 2: Implement and regression-test the state lifecycle

**Files:**
- Modify: `lib/temperature_engine/include/temperature_engine.hpp:38-95`
- Modify: `lib/temperature_engine/src/temperature_engine.cpp:18-220`
- Modify: `test/test_temperature_engine/test_temperature_engine.cpp:14-173`

**Interfaces:**
- Consumes: `TemperatureEngine::ingest(const TemperatureSample&)`.
- Produces: deterministic `opened`, `escalated`, `reminder`, and `resolved` events.
- Adds policy fields `rapid_change_recovery_duration_ms`, `gradient_recovery_c`, and `gradient_recovery_duration_ms`, defaulted to 30 minutes, `0.6`, and 10 minutes.

- [ ] **Step 1: Write failing lifecycle tests**

Add focused assertions for all of these traces:

```cpp
// Rapid N2 opens at 30m, remains <1.0C for another full 30m, then resolves once.
// A later 1.0C change opens a new rapid incident.
// An N2 rapid incident reaching 2.0C emits one N3 EventState::escalated and
// ultimately resolves once as N3.
// A 0.9C gradient opens at 10m; 0.6C begins recovery; 0.7C resets recovery;
// a fresh 10m at 0.6C resolves exactly once.
// Two invalid main samples open sensor fault; temperature reminders are absent
// while invalid; sensor fault reminds after 60m; a valid normal sample resolves
// only sensor fault, and a prior high-temperature incident waits for a new 15m
// valid recovery interval before resolving.
```

- [ ] **Step 2: Run the focused engine test and verify the new assertions fail**

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests
```

Expected: assertion failure from missing recovery, escalation, or sensor-fault reminder behavior.

- [ ] **Step 3: Implement the minimum state transitions**

Add state for rapid peak severity, rapid recovery start, gradient recovery start, and notification timestamps. On invalid main samples, reset candidate/recovery timers and rapid history, emit only sensor-fault open/reminder events, then return. On valid samples, resolve sensor fault first and evaluate high/low recovery from the first valid sample after interruption.

For rapid movement, calculate current window difference before appending the sample. Emit N2/N3 `opened` based on the first threshold crossed; if an N2 incident reaches N3, emit exactly one N3 `escalated`. Start or reset 30-minute recovery based on movement `<1.0°C`.

For gradient, retain the existing 10-minute open candidate and add a separate `<=0.6°C` 10-minute recovery candidate plus one-hour reminder timing.

- [ ] **Step 4: Re-run focused engine and simulation tests**

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_simulation.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_simulation_tests && ./.build/temperature_simulation_tests
```

Expected: both executables report their pass messages.

### Task 3: Align documentation and run the local completion gate

**Files:**
- Modify: `README.md:8-23`
- Modify: `HANDOVER_SOFTWARE_20260716.md:17-48`
- Modify: `test/verify_docs.sh` only if a required documented contract has no existing assertion.

**Interfaces:**
- Consumes: implemented event names and state strings.
- Produces: user-facing documentation naming rapid escalation, recovery semantics, and fault continuity behavior without credentials.

- [ ] **Step 1: Update the behavior table and reminder wording**

Document rapid N2/N3 escalation and 30-minute stability recovery, gradient `<=0.6°C`/10-minute recovery, and sensor-fault reminder plus invalid-sample continuity rules. Keep all security and hardware-placement guidance unchanged.

- [ ] **Step 2: Run the complete local regression suite**

```sh
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_simulation.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_simulation_tests && ./.build/temperature_simulation_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include test/test_temperature_engine/test_config.cpp -o .build/config_tests && ./.build/config_tests
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && ./.build/transport_contract_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/heartbeat_contract/include test/test_heartbeat_contract/test_heartbeat_contract.cpp lib/heartbeat_contract/src/heartbeat_contract.cpp -o .build/heartbeat_contract_tests && ./.build/heartbeat_contract_tests
node --test cloud/bark-forwarder/index.test.mjs
(cd cloud/tencent-scf && npm test)
sh test/verify_docs.sh
/Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8
```

Expected: every command exits zero. The S3 build is compilation evidence only, not a real-board claim.
