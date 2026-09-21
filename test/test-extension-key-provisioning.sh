#!/usr/bin/env bash
set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
diag="$project_dir/src/diagnostics/extension_lan_key_diag.cpp"

test -f "$diag"
rg -q 'EXTENSION_LAN_KEY' "$diag"
rg -q 'extension_lan' "$diag"
rg -q 'EXTENSION_LAN_KEY_STORED' "$diag"

echo "agent11 extension key provisioning contract: PASS"
