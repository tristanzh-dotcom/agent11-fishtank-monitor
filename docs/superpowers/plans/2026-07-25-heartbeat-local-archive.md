# 5-Minute Heartbeat Local Archive Implementation Plan

> **For agentic workers:** Execute this plan inline with test-first verification. The collector must remain independent of Codex at runtime.

**Goal:** Poll the deployed Agent11 read-only state every five minutes and append each validated raw `FishTankStateV1` response to a local JSONL archive inside the configured Obsidian Vault.

**Architecture:** A dependency-free Python collector reads the existing Agent12 secure environment file for the HTTPS base URL, device ID, and read token, performs one GET, validates the response shape without changing it, and atomically appends a record containing collector metadata plus the exact upstream JSON. A launchd template runs the collector every five minutes; Obsidian reads the JSONL directly or through a daily Markdown projection generated on demand.

**Tech Stack:** Python 3 standard library, HTTPS `urllib`, JSONL, macOS launchd.

## Global Constraints

- Agent11, ESP32, SCF, COS, and Agent12 protocols remain unchanged.
- No Token, URL with credentials, or secret is written to the Vault, logs, or stdout.
- The collector is read-only against Agent11 and appends only local archive files.
- Missing or invalid upstream data is recorded as an error record; no fabricated temperatures are written.
- The Vault path and archive folder are explicit CLI arguments or environment variables; no guessed path.

### Task 1: Collector Contract Tests

**Files:**
- Create: `tools/test_heartbeat_archive.py`
- Create: `tools/heartbeat_archive.py`

- [ ] **Step 1: Write failing tests** for strict response validation, raw JSON preservation, one-record append, and secret-safe error output.
- [ ] **Step 2: Run `python3 -m unittest tools.test_heartbeat_archive` and confirm the missing module failure.**
- [ ] **Step 3: Implement `parse_state_response`, `build_archive_record`, `append_jsonl`, and `collect_once` with standard-library HTTPS and explicit config injection.**
- [ ] **Step 4: Re-run the focused tests and require all tests to pass.**

### Task 2: Runtime Configuration and Launchd Template

**Files:**
- Create: `tools/com.tz.agent11.heartbeat-archive.plist.template`
- Modify: `README.md`

- [ ] **Step 1: Add a launchd template that invokes the collector every 300 seconds with explicit Vault and secure environment-file paths.**
- [ ] **Step 2: Document installation, dry-run, archive layout, permissions, and the limitation that the archive contains five-minute heartbeats rather than 30-second samples.**
- [ ] **Step 3: Run the collector once against a temporary Vault and a test HTTP server; inspect the JSONL record and verify no secret appears in it.**

### Task 3: Verification

- [ ] **Step 1: Run all collector unit tests.**
- [ ] **Step 2: Run the existing Agent11 cloud tests without changing cloud code.**
- [ ] **Step 3: Run `git diff --check` and inspect status; do not stage or commit.**
- [ ] **Step 4: Report the archive path, installation command, checks, and any live GET limitation.**
