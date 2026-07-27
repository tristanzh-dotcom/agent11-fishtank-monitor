#!/usr/bin/env python3
"""Archive validated Agent11 five-minute heartbeat snapshots locally."""

import argparse
import json
import re
import shlex
import sys
import time
import urllib.error
import urllib.request
from dataclasses import dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Callable, Dict, Mapping, Optional, Union


class ArchiveError(ValueError):
    """Raised when an upstream response cannot be safely archived."""


@dataclass(frozen=True)
class CollectorConfig:
    base_url: str
    device_id: str
    token: str
    archive_path: Path
    timeout_s: float = 15.0


_EVENT_TYPES = {
    "high_temperature",
    "high_temperature_critical",
    "low_temperature",
    "low_temperature_critical",
    "temperature_rapid_change",
    "temperature_gradient",
    "sensor_fault",
    "network_offline",
    "network_recovered",
    "heartbeat_missing",
}
_EVENT_STATES = {"opened", "escalated", "reminder"}
_SEVERITIES = {"n2", "n3"}
_DEVICE_ID = re.compile(r"^[a-z0-9][a-z0-9_-]{0,31}$")


def _number(value: Any, name: str) -> None:
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise ArchiveError(f"invalid {name}")


def _temperature(value: Any, name: str) -> None:
    if value is None:
        return
    _number(value, name)
    if isinstance(value, float) and (value != value or value in (float("inf"), float("-inf"))):
        raise ArchiveError(f"invalid {name}")


def parse_state_response(body: Union[str, bytes, Mapping[str, Any]], expected_device_id: str) -> Dict[str, Any]:
    if isinstance(body, bytes):
        body = body.decode("utf-8")
    try:
        state = json.loads(body) if isinstance(body, str) else dict(body)
    except (TypeError, ValueError, UnicodeDecodeError) as exc:
        raise ArchiveError("invalid JSON response") from exc
    if not isinstance(state, dict):
        raise ArchiveError("response is not an object")
    required = {"schema_version", "device_id", "timestamp_ms", "display_c", "return_c", "connectivity_status", "events"}
    if set(state) != required:
        raise ArchiveError("response fields do not match FishTankStateV1")
    if state["schema_version"] != 1 or state["device_id"] != expected_device_id:
        raise ArchiveError("unexpected schema or device")
    if not _DEVICE_ID.fullmatch(expected_device_id):
        raise ArchiveError("invalid device id")
    _number(state["timestamp_ms"], "timestamp_ms")
    if not isinstance(state["timestamp_ms"], int) or state["timestamp_ms"] < 0:
        raise ArchiveError("invalid timestamp_ms")
    _temperature(state["display_c"], "display_c")
    _temperature(state["return_c"], "return_c")
    if state["connectivity_status"] not in {"online", "offline", "unknown"}:
        raise ArchiveError("invalid connectivity_status")
    if not isinstance(state["events"], list) or len(state["events"]) > 7:
        raise ArchiveError("invalid events")
    seen = set()
    for event in state["events"]:
        if not isinstance(event, dict) or set(event) != {"type", "state", "severity", "at_ms", "display_c"}:
            raise ArchiveError("invalid event shape")
        if event["type"] not in _EVENT_TYPES or event["state"] not in _EVENT_STATES or event["severity"] not in _SEVERITIES:
            raise ArchiveError("invalid event enum")
        if event["type"] in seen:
            raise ArchiveError("duplicate event type")
        seen.add(event["type"])
        _number(event["at_ms"], "event at_ms")
        if not isinstance(event["at_ms"], int) or event["at_ms"] < 0:
            raise ArchiveError("invalid event at_ms")
        _temperature(event["display_c"], "event display_c")
        if event["type"] == "sensor_fault" and event["display_c"] is not None:
            raise ArchiveError("sensor_fault display_c must be null")
    return state


def build_archive_record(state: Mapping[str, Any], collected_at_ms: int) -> Dict[str, Any]:
    return {
        "archive_schema_version": 1,
        "collected_at_ms": collected_at_ms,
        "collected_at": datetime.fromtimestamp(collected_at_ms / 1000, tz=timezone.utc).isoformat().replace("+00:00", "Z"),
        "source": "agent11_state_api",
        "state": dict(state),
    }


def append_jsonl(path: Path, record: Mapping[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    try:
        path.parent.chmod(0o700)
    except OSError:
        pass
    with path.open("a", encoding="utf-8") as handle:
        handle.write(json.dumps(record, ensure_ascii=False, separators=(",", ":")) + "\n")
    try:
        path.chmod(0o600)
    except OSError:
        pass


def collect_once(
    config: CollectorConfig,
    opener: Callable[..., Any] = urllib.request.urlopen,
    now_ms: Callable[[], int] = lambda: int(time.time() * 1000),
) -> Dict[str, Any]:
    if not config.base_url.startswith("https://"):
        raise ArchiveError("Agent11 base URL must use HTTPS")
    if not _DEVICE_ID.fullmatch(config.device_id):
        raise ArchiveError("invalid device id")
    request = urllib.request.Request(
        f"{config.base_url.rstrip('/')}/api/v1/devices/{config.device_id}/state",
        headers={"Accept": "application/json", "Authorization": f"Bearer {config.token}"},
        method="GET",
    )
    try:
        with opener(request, timeout=config.timeout_s) as response:
            if getattr(response, "status", 200) != 200:
                raise ArchiveError(f"Agent11 returned HTTP {getattr(response, 'status', 'unknown')}")
            state = parse_state_response(response.read(), config.device_id)
    except urllib.error.HTTPError as exc:
        raise ArchiveError(f"Agent11 returned HTTP {exc.code}") from exc
    except (urllib.error.URLError, TimeoutError, OSError) as exc:
        raise ArchiveError(f"Agent11 request failed: {type(exc).__name__}") from exc
    record = build_archive_record(state, now_ms())
    append_jsonl(config.archive_path, record)
    return record


def load_env_file(path: Path) -> Dict[str, str]:
    values: Dict[str, str] = {}
    for raw_line in path.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue
        if line.startswith("export "):
            line = line[7:].lstrip()
        if "=" not in line:
            continue
        key, raw_value = line.split("=", 1)
        key = key.strip()
        if not re.fullmatch(r"[A-Z0-9_]+", key):
            continue
        try:
            parsed = shlex.split(raw_value, comments=False, posix=True)
        except ValueError as exc:
            raise ArchiveError(f"invalid env file line for {key}") from exc
        values[key] = parsed[0] if parsed else ""
    return values


def main(argv: Optional[list] = None) -> int:
    parser = argparse.ArgumentParser(description="Archive Agent11 five-minute state snapshots into Obsidian JSONL")
    parser.add_argument("--env-file", type=Path, required=True)
    parser.add_argument("--vault", type=Path, required=True)
    parser.add_argument("--archive-folder", default="95_Ledgers/fishtank/heartbeat")
    parser.add_argument("--timeout", type=float, default=15.0)
    args = parser.parse_args(argv)
    try:
        env = load_env_file(args.env_file)
        base_url = env.get("AGENT11_STATE_API_BASE_URL", "")
        token = env.get("AGENT11_WEB_READ_TOKEN", "")
        device_id = env.get("AGENT11_DEVICE_ID", "tank01")
        if not base_url or not token:
            raise ArchiveError("required Agent11 configuration is missing")
        date_name = datetime.now().date().isoformat()
        archive_path = args.vault / args.archive_folder / f"heartbeat-{date_name}.jsonl"
        record = collect_once(CollectorConfig(base_url, device_id, token, archive_path, args.timeout))
        print(f"archived source_timestamp_ms={record['state']['timestamp_ms']} path={archive_path}")
        return 0
    except (ArchiveError, OSError) as exc:
        print(f"collector_error={exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
