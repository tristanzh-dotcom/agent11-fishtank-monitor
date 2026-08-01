# Agent11 Fish-Tank Monitor Governance

Scope: this repository, including firmware and the Tencent SCF package.

The current README, hardware-live handoff, cloud-shift handoff, contracts, and
tests define local behavior. Software evidence, cloud deployment, physical
commissioning, and sustained observation are separate acceptance stages.

## Safety and evidence boundaries

- This system observes temperature and reports events. It does not control
  heaters, pumps, mains power, dosing, or other aquarium actuators.
- Preserve animal-safety thresholds, persistence durations, sensor-fault
  behavior, event lifecycle, retry/idempotency, and `main_tank`/`sump_tank`
  identity. A change to these semantics requires explicit product approval.
- Wi-Fi, Bark, Tencent, TLS, and device credentials remain local or server-side.
  Never print, commit, embed in URLs, or export them.
- Real sensor values, device identity, cloud receipt, Bark delivery, and
  installation evidence must be measured. Missing live data stays missing or
  `null`; fixtures and simulation are not production evidence.
- Firmware build, upload, serial observation, tank installation, calibration,
  and 48–72-hour observation are distinct gates. Do not infer later gates from
  an earlier pass.

## Verification and acceptance

Use the README and current manifests to select only the affected temperature,
config, transport, heartbeat, SCF, documentation, or firmware layer. A firmware
change may require its PlatformIO target build; an SCF change may require its
focused Node tests. Documentation-only changes remain Level 0.

Run the full cloud package, all host contract binaries, or cross-layer build only
for a Workspace Level 4 trigger or approved release/commissioning gate. Upload
still requires the exact approved device and `--upload-port`; never flash
hardware merely to increase test coverage. Completion must name the software,
cloud, upload, installation, calibration, and observation gates actually
exercised.
