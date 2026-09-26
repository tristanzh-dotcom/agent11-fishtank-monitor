#!/usr/bin/env python3
"""Bounded, metadata-only ESP1/ESP3 serial and TGR1 LAN observation."""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import socket
import subprocess
import tempfile
import time

import serial


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--esp1', required=True)
    parser.add_argument('--esp3', required=True)
    parser.add_argument('--seconds', type=int, default=300)
    parser.add_argument('--air-after', type=int)
    parser.add_argument('--air-duration', type=int, default=300)
    args = parser.parse_args()
    for port in (args.esp1, args.esp3):
        owner = subprocess.run(['lsof', '-nP', port], capture_output=True, text=True)
        if owner.stdout.strip():
            raise RuntimeError('serial target busy')
    fd, path = tempfile.mkstemp(prefix='esp3_delivery_', suffix='.jsonl')
    os.chmod(path, 0o600)
    print('METADATA_LOG=' + path, flush=True)
    allowed = re.compile(r'^(DIAG(?:_| )|ESP1_(?:FIRMWARE|WIFI)|ESP3_(?:TX |DIAG|FIRMWARE|WIFI|PROBE)|GRASS_LAN_|EXTENSION_LAN_|AIR_)')
    sensitive = re.compile(r'(?:\d{1,3}\.){3}\d{1,3}|(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}|[0-9a-fA-F]{32,}')
    ports = {}
    udp = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    udp.bind(('', 35115))
    udp.setblocking(False)
    buffers = {'ESP1': b'', 'ESP3': b''}
    start = time.monotonic()
    next_diag = 0
    air_started = air_stopped = False
    with os.fdopen(fd, 'w') as output:
        def emit(source, **data):
            row = dict(at=datetime.datetime.now().astimezone().isoformat(), elapsed=round(time.monotonic()-start, 3), source=source, **data)
            line = json.dumps(row)
            output.write(line+'\n')
            output.flush()
            if source == 'UDP' or 'DIAG_RX source=' in data.get('line', '') or 'ESP3_TX' in data.get('line', '') or 'RSSI' in data.get('line', ''):
                print(line, flush=True)
        try:
            for role, port in (('ESP1', args.esp1), ('ESP3', args.esp3)):
                connection = serial.Serial(port=None, baudrate=115200, timeout=0)
                connection.dtr = False
                connection.rts = False
                connection.port = port
                connection.open()
                ports[role] = connection
            while time.monotonic()-start < args.seconds:
                elapsed = time.monotonic()-start
                if args.air_after is not None and elapsed >= args.air_after and not air_started:
                    identity = json.loads((Path.home()/'.config/agent-hardware/esp3-identity.json').read_text())
                    if identity.get('role') != 'ESP3' or identity.get('chip') != 'ESP32-C3':
                        raise RuntimeError('invalid private ESP3 registry')
                    target = identity['mcu_mac'].replace(':', '')
                    if not re.fullmatch('[0-9a-fA-F]{12}', target):
                        raise RuntimeError('invalid private ESP3 identity')
                    ports['ESP1'].write(('AIR '+target+'\n').encode())
                    air_started = True
                    emit('CONTROL', action='air_start_requested')
                if air_started and not air_stopped and elapsed >= args.air_after+args.air_duration:
                    ports['ESP1'].write(b'AIR_OFF\n')
                    air_stopped = True
                    emit('CONTROL', action='air_stop_requested')
                if time.monotonic()-start >= next_diag:
                    for connection in ports.values():
                        connection.write(b'DIAG\n')
                    next_diag += 30
                for role, connection in ports.items():
                    buffers[role] += connection.read(8192)
                    while b'\n' in buffers[role]:
                        raw, buffers[role] = buffers[role].split(b'\n', 1)
                        line = raw.decode('utf-8', errors='replace').strip()
                        if allowed.match(line):
                            emit(role, line=sensitive.sub('[masked]', line))
                    if len(buffers[role]) > 16384:
                        buffers[role] = b''
                while True:
                    try:
                        data, _ = udp.recvfrom(2048)
                    except BlockingIOError:
                        break
                    if len(data) == 72 and data[:5] == b'TGR1\x01':
                        emit('UDP', seq=int.from_bytes(data[16:20], 'big'), uptime_ms=int.from_bytes(data[20:28], 'big'))
                time.sleep(0.02)
            emit('END', duration=args.seconds)
        finally:
            if air_started and not air_stopped and 'ESP1' in ports:
                ports['ESP1'].write(b'AIR_OFF\n')
                ports['ESP1'].flush()
            for connection in ports.values():
                connection.close()
            udp.close()


if __name__ == '__main__':
    main()
