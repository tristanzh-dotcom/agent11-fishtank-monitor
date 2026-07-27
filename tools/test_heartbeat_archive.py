import json
import tempfile
import unittest
from pathlib import Path

from tools.heartbeat_archive import (
    CollectorConfig,
    ArchiveError,
    append_jsonl,
    build_archive_record,
    collect_once,
    parse_state_response,
)


VALID_STATE = {
    "schema_version": 1,
    "device_id": "tank01",
    "timestamp_ms": 1784992273288,
    "display_c": 24.75,
    "return_c": 24.81,
    "connectivity_status": "online",
    "events": [],
}


class Response:
    status = 200

    def read(self):
        return json.dumps(VALID_STATE).encode("utf-8")

    def __enter__(self):
        return self

    def __exit__(self, *_args):
        return False


class HeartbeatArchiveTests(unittest.TestCase):
    def test_parse_state_response_preserves_valid_snapshot(self):
        parsed = parse_state_response(json.dumps(VALID_STATE), "tank01")
        self.assertEqual(parsed, VALID_STATE)

    def test_parse_state_response_rejects_wrong_device_and_invalid_sensor_fault(self):
        wrong_device = {**VALID_STATE, "device_id": "tank99"}
        with self.assertRaises(ArchiveError):
            parse_state_response(json.dumps(wrong_device), "tank01")

        invalid_fault = {
            **VALID_STATE,
            "events": [{
                "type": "sensor_fault",
                "state": "opened",
                "severity": "n2",
                "at_ms": 10,
                "display_c": 24.0,
            }],
        }
        with self.assertRaises(ArchiveError):
            parse_state_response(json.dumps(invalid_fault), "tank01")

    def test_append_jsonl_writes_one_record_with_restricted_mode(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "2026-07-25.jsonl"
            record = build_archive_record(VALID_STATE, 1784993307000)
            append_jsonl(path, record)
            append_jsonl(path, record)
            lines = path.read_text(encoding="utf-8").splitlines()
            self.assertEqual(len(lines), 2)
            self.assertEqual(json.loads(lines[0]), record)
            self.assertEqual(path.stat().st_mode & 0o777, 0o600)

    def test_collect_once_appends_exact_upstream_state_without_printing_token(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "heartbeat.jsonl"
            config = CollectorConfig(
                base_url="https://example.test/function",
                device_id="tank01",
                token="secret-token-that-must-not-be-written",
                archive_path=path,
                timeout_s=7.0,
            )

            def opener(request, timeout):
                self.assertEqual(request.get_method(), "GET")
                self.assertEqual(request.headers["Authorization"], "Bearer secret-token-that-must-not-be-written")
                self.assertEqual(timeout, 7.0)
                return Response()

            record = collect_once(config, opener=opener, now_ms=lambda: 1784993307000)
            text = path.read_text(encoding="utf-8")
            self.assertEqual(record["state"], VALID_STATE)
            self.assertNotIn(config.token, text)
            self.assertIn(json.dumps(VALID_STATE, separators=(",", ":")), text)


if __name__ == "__main__":
    unittest.main()
