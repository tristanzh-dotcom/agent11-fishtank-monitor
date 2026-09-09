# Multi-Tank Bark Temperature Monitor Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add three independently monitored local DS18B20 channels and two daily five-reading Bark summaries while preserving the existing main/sump public heartbeat and Web contracts.

**Architecture:** Keep the existing TemperatureEngine, main/sump TemperatureSample, ActiveEventSnapshot, heartbeat, and main Bark queue for the current pair. Add three independent TemperatureEngine instances driven by TemperatureSample{at_ms, auxiliary_c, nullopt}, a bounded scoped-event queue for auxiliary Bark messages, and a host-testable daily summary scheduler with one pending snapshot and a ten-minute expiry. Integrate the new reader result into firmware only after host contracts are green; do not write the three physical ROMs or upload hardware.

**Tech Stack:** C++17 host contract binaries, Arduino/PlatformIO ESP32-S3 firmware, existing OneWire/DallasTemperature reader, existing HTTPS Bark notifier, existing shell documentation check.

## Global Constraints

- Preserve the existing main/sump thresholds, event lifecycle, heartbeat payload, FishTankStateV1, Tencent SCF projection, and Agent12/Web page.
- Use old-four low-temperature thresholds <22.5°C / <20.5°C with low recovery >=23.5°C.
- Use small-tank high-temperature thresholds >27.5°C / >28.5°C and existing duration/reminder rules.
- Run daily summary slots only in [09:00:00, 09:01:00) and [18:00:00, 18:01:00) in Asia/Shanghai local time.
- Preserve the sampled five-reading snapshot across Bark retries; expire an undelivered summary ten minutes after generation.
- Do not retry a missed slot, create NVS persistence, add a cloud scheduler, or expose auxiliary readings to Web/SCF.
- Keep main Bark capacity at 16, auxiliary scoped-event capacity at 16, and the daily summary at one pending slot.
- Auxiliary ROMs must be configured and unique; never fall back to OneWire discovery order for an auxiliary role.
- sensor_fault Bark text uses “无有效读数” and never prints the internal 0.0 event placeholder.
- Do not commit or upload from this implementation session; leave selected working-tree changes reviewable.

---

### Task 1: Extend configuration with three named auxiliary channels

**Files:**

- Modify: include/config.hpp
- Test: test/test_temperature_engine/test_config.cpp

**Interfaces:**

- Add AuxiliaryTankConfig, SensorBindingState, auxiliary_sensor_binding_state(const RuntimeConfig&, std::size_t), and sensor_roms_unique(const RuntimeConfig&).
- Add RuntimeConfig::auxiliary_tanks as a fixed three-element array with keys laosi_tank, xiaohei_tank, and maomao_tank.

- [ ] Write failing assertions for labels, policies, zero new ROMs, unconfigured state, and duplicate state.

~~~cpp
assert(std::string(config.auxiliary_tanks[0].key) == "laosi_tank");
assert(std::string(config.auxiliary_tanks[0].label) == "老四缸");
assert(config.auxiliary_tanks[0].policy.low_attention_c == 22.5);
assert(config.auxiliary_tanks[0].policy.low_critical_c == 20.5);
assert(config.auxiliary_tanks[0].policy.low_recovery_c == 23.5);
assert(!config.auxiliary_tanks[0].sensor.configured());
assert(auxiliary_sensor_binding_state(config, 0) ==
       SensorBindingState::unconfigured);
assert(!sensor_roms_unique(config));
~~~

- [ ] Run the focused configuration binary and verify it fails because the new fields and helpers do not exist.
- [ ] Implement the three fixed entries with zero ROMs, the approved labels, and per-tank policies. Compare each configured auxiliary ROM against both primary ROMs and all other auxiliary ROMs. An unconfigured address returns unconfigured; a configured duplicate returns duplicate; otherwise return valid.
- [ ] Add assertions for three unique test ROMs returning valid and an auxiliary ROM equal to the main ROM returning duplicate.
- [ ] Run the focused configuration binary again; expect firmware configuration tests passed.

Command:

~~~sh
g++ -std=c++17 -Wall -Wextra -Werror -Iinclude -Ilib/temperature_engine/include test/test_temperature_engine/test_config.cpp -o .build/config_tests && ./.build/config_tests
~~~

### Task 2: Add independent auxiliary event identity and bounded queue

**Files:**

- Modify: lib/transport_contract/include/transport_contract.hpp
- Modify: lib/transport_contract/src/transport_contract.cpp
- Create: lib/transport_contract/include/scoped_event_outbox.hpp
- Create: lib/transport_contract/src/scoped_event_outbox.cpp
- Modify: test/test_temperature_engine/test_temperature_engine.cpp
- Modify: test/test_transport_contract/test_transport_contract.cpp

**Interfaces:**

- Add ScopedTemperatureEvent with tank_key, tank_label, and TemperatureEvent event.
- Add ScopedEventOutbox(std::size_t), push, front, pop, empty, and dropped_count.
- Keep TemperatureEngine::ingest unchanged; the firmware owns one engine per auxiliary channel.

- [ ] Add a failing engine test that feeds old-four low readings to one engine and normal readings to the other two. Use TemperatureSample{at_ms, auxiliary_value, std::nullopt}; assert no auxiliary gradient event.
- [ ] Add a failing queue test with same event type and different tank keys. Assert FIFO order, capacity 16, rejection of the seventeenth item, and preservation of the old front item.
- [ ] Run the existing temperature and transport commands and confirm the new cases fail for the intended missing interfaces or behavior.
- [ ] Implement ScopedEventOutbox using std::deque and the existing EventOutbox capacity semantics. Do not add deduplication, eviction, threads, or generic fan-out.
- [ ] Add scoped Bark formatting that includes tank_label in title/body. Treat any fingerprint as metadata only; do not claim it performs deduplication or add it to the existing request JSON.
- [ ] Run the focused engine and transport commands and confirm all old and new assertions pass.

~~~sh
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include test/test_temperature_engine/test_temperature_engine.cpp lib/temperature_engine/src/temperature_engine.cpp -o .build/temperature_engine_tests && ./.build/temperature_engine_tests
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/temperature_engine/include -Ilib/transport_contract/include test/test_transport_contract/test_transport_contract.cpp lib/temperature_engine/src/temperature_engine.cpp lib/transport_contract/src/transport_contract.cpp lib/transport_contract/src/event_outbox.cpp lib/transport_contract/src/scoped_event_outbox.cpp lib/transport_contract/src/retry_backoff.cpp lib/transport_contract/src/delivery_coordinator.cpp -o .build/transport_contract_tests && ./.build/transport_contract_tests
~~~

### Task 3: Add daily summary snapshot, formatter, and scheduler

**Files:**

- Create: lib/transport_contract/include/daily_summary.hpp
- Create: lib/transport_contract/src/daily_summary.cpp
- Create: test/test_daily_summary/test_daily_summary.cpp
- Modify: lib/transport_contract/include/transport_contract.hpp
- Modify: lib/transport_contract/src/transport_contract.cpp
- Modify: test/test_transport_contract/test_transport_contract.cpp

**Interfaces:**

- LocalDateTime contains year, month, day, hour, minute, second, and valid.
- SummaryReadingState is valid, invalid, unconfigured, or configuration_error.
- DailyTemperatureSnapshot contains sampled_at, main_c, sump_c, three auxiliary values, and three auxiliary states.
- DailySlot is morning or evening. DailyTemperatureSummary contains slot_key, slot, and snapshot.
- DailySummaryScheduler exposes observe(local_time, monotonic_ms, snapshot), pending(), acknowledge(bool), and expire(monotonic_ms).
- Add daily_summary_body(summary) and daily_summary_message(summary, aquarium_id).

- [ ] Add failing tests for the two one-minute windows, first sample in a window, invalid time, missed window, continuous-boot slot suppression, time rollback, forward jump, ten-minute expiry, failed acknowledgement retaining the exact snapshot, and successful acknowledgement clearing the pending slot.
- [ ] Add formatter assertions for all five labels, sample time, “无有效读数”, “未配置”, “配置错误”, and Bark level active.
- [ ] Run the new test and verify it fails because the scheduler and formatter do not exist.
- [ ] Implement only the two fixed windows. Generate a slot key at creation, retain one pending snapshot, retain the exact snapshot after failed acknowledgement, and expire it at monotonic generation time plus 600000 ms. Keep the generated slot key for the current boot. Do not catch up missed windows or do host timezone arithmetic.
- [ ] Run the daily summary command and the transport command.

~~~sh
g++ -std=c++17 -Wall -Wextra -Werror -Ilib/transport_contract/include test/test_daily_summary/test_daily_summary.cpp lib/transport_contract/src/daily_summary.cpp lib/transport_contract/src/transport_contract.cpp -o .build/daily_summary_tests && ./.build/daily_summary_tests
~~~

### Task 4: Extend the reader and enforce fixed ROM reads

**Files:**

- Modify: include/config.hpp
- Modify: src/ds18b20_reader.hpp
- Modify: src/ds18b20_reader.cpp
- Modify: test/test_temperature_engine/test_config.cpp

**Interfaces:**

- Add TemperatureReadings with at_ms, primary TemperatureSample, three auxiliary optional temperatures, and three SensorBindingState values.
- Change Ds18b20Reader::read(std::uint64_t) to return TemperatureReadings.
- Change the private read_sensor function to accept only a configured SensorRomAddress; it must never accept a discovery index fallback.

- [ ] Extend the configuration test with three unique nonzero test ROMs returning valid and an auxiliary address equal to the main ROM returning duplicate.
- [ ] Run the configuration command and verify the new assertions fail before the reader/config implementation is complete.
- [ ] Implement one requestTemperatures call, fixed-address reads for main and sump, binding-state checks for auxiliaries, and nullopt for unconfigured, duplicate, disconnected, or out-of-range readings.
- [ ] Keep the existing ROM discovery printout for local identification; never use discovery order for a reading.
- [ ] Run the configuration command and compile the PlatformIO target in Task 6 to verify the Arduino reader signature.

### Task 5: Integrate auxiliary alerts and daily summary into firmware

**Files:**

- Modify: src/main.cpp
- Modify: src/bark_notifier.hpp
- Modify: src/bark_notifier.cpp
- Modify: src/heartbeat_notifier.cpp
- Modify: test/test_transport_contract/test_transport_contract.cpp

**Interfaces:**

- Add BarkNotifier overloads for ScopedTemperatureEvent and DailyTemperatureSummary while retaining the current main event overload.
- Keep HeartbeatNotifier::notify inputs and serialized payload unchanged.

- [ ] Add failing transport assertions for tank names, “无有效读数”, absence of 0°C fault text, daily level active, and byte-for-byte compatibility of the current main event request.
- [ ] Run the transport command and verify the new assertions fail.
- [ ] Initialize three TemperatureEngine objects from runtime_config.auxiliary_tanks[i].policy, one scoped queue of capacity 16, and one DailySummaryScheduler.
- [ ] Preserve readings.primary for the existing engine and heartbeat. Feed only valid auxiliary roles into their independent engines, wrap events with configured key/label, and do not call ActiveEventSnapshot::apply for auxiliary events.
- [ ] Use a small round-robin selector between the main Bark queue and the auxiliary queue. Attempt at most one alert per sampling loop. A failed attempt retains the selected front. Offer the daily summary only when both alert queues are empty and no alert was attempted in that loop.
- [ ] Convert synchronized epoch time to LocalDateTime using fixed UTC+8. Generate the snapshot in the approved one-minute windows. Use a one-slot pending summary and expire it after ten minutes. Keep the heartbeat request limited to the existing main/sump values and main event snapshot.
- [ ] Format fault opened/reminder as “无有效读数”, never the internal 0.0 placeholder; resolved fault uses the recovered value. Set five sensor labels in the summary in fixed order.
- [ ] Set HTTPS connect/response timeouts to the existing five-second bounds and confirm a failed request returns to the loop without an unbounded wait.
- [ ] Run the PlatformIO target build. No upload, physical ROM write, real Bark call, or hardware integration.

~~~sh
/Users/tristanzh/Library/Python/3.9/bin/pio run -e waveshare_esp32s3_n16r8
~~~

### Task 6: Update README and run bounded pre-hardware verification

**Files:**

- Modify: README.md
- Test: test/verify_docs.sh

**Interfaces:**

- README states the three auxiliary channels are local Bark-only, the public two-zone heartbeat/Web contract remains unchanged, the real auxiliary ROMs are unset until hardware identification, and the summary window/expiry rules.
- Do not modify cloud/tencent-scf, docs/contracts, or Agent12/Web files.

- [ ] Add one concise operational section without claiming hardware acceptance.
- [ ] Run sh test/verify_docs.sh.
- [ ] Run the focused configuration, temperature-engine, transport, daily-summary, heartbeat, active-event-snapshot, and existing simulation binaries from README.md.
- [ ] Run the PlatformIO target build once after all firmware changes.
- [ ] Run git diff --check, git status --short, and git diff --stat. Confirm no secret, real auxiliary ROM, SCF/Web contract, upload, or hardware state changed. Leave the worktree uncommitted for review.

## Verification Summary

Pre-hardware completion requires focused host tests, the documentation validator, and the PlatformIO target build to pass. Physical five-ROM identity, long-line OneWire stability, real Bark receipt, Wi-Fi/NTP behavior on the board, sensor installation, calibration, upload, and 48–72-hour observation remain unverified.

## Plan Self-Review

- Spec coverage: configuration and ROM safety are Tasks 1 and 4; independent engines and scoped events are Task 2; daily windows, snapshots, expiry, and formatting are Task 3; firmware integration and queue ordering are Task 5; docs and bounded verification are Task 6.
- Boundary check: no task modifies the public heartbeat payload, FishTankStateV1, SCF, Agent12/Web, cloud scheduling, NVS, or physical device state.
- Type check: TemperatureReadings, ScopedTemperatureEvent, DailyTemperatureSnapshot, DailyTemperatureSummary, and DailySummaryScheduler are defined before their consumers.
- Placeholder scan: no task depends on an unspecified threshold, ROM, queue capacity, retry window, or hardware port.
