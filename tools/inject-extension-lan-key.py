#!/usr/bin/env python3
"""Write the TEX1 key into ESP1's extension_lan NVS namespace."""

from __future__ import annotations

import argparse
import re
import sys
import time
from pathlib import Path

KEY_FILE = Path("/Users/tristanzh/agent/agent11-fishtank-extension/include/secrets.hpp")
KEY_RE = re.compile(r"kExtensionLanKeyHex\[\]\s*=\s*\"([0-9a-fA-F]{64})\"")


def load_key() -> str:
    try:
        match = KEY_RE.search(KEY_FILE.read_text(encoding="ascii"))
    except OSError as error:
        raise RuntimeError("EXTENSION_LAN_KEY_SOURCE_UNAVAILABLE") from error
    if match is None:
        raise RuntimeError("EXTENSION_LAN_KEY_SOURCE_INVALID")
    return match.group(1)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", required=True)
    args = parser.parse_args()
    try:
        import serial  # type: ignore
    except ImportError:
        print("EXTENSION_LAN_KEY_PYSERIAL_UNAVAILABLE", file=sys.stderr)
        return 2
    key = load_key()
    with serial.Serial(args.port, 115200, timeout=0.25, write_timeout=2) as device:
        deadline = time.monotonic() + 10
        while time.monotonic() < deadline:
            if device.readline().strip() == b"EXTENSION_LAN_KEY_DIAG_READY":
                break
        else:
            print("EXTENSION_LAN_KEY_DIAG_NOT_READY", file=sys.stderr)
            return 1
        device.write(f"EXTENSION_LAN_KEY {key}\n".encode("ascii"))
        device.flush()
        while time.monotonic() < deadline:
            marker = device.readline().strip()
            if marker == b"EXTENSION_LAN_KEY_STORED":
                print("EXTENSION_LAN_KEY_INJECTION_OK")
                return 0
            if marker in (b"EXTENSION_LAN_KEY_REJECTED",
                          b"EXTENSION_LAN_KEY_STORE_FAILED"):
                print(marker.decode("ascii"), file=sys.stderr)
                return 1
    print("EXTENSION_LAN_KEY_INJECTION_TIMEOUT", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
