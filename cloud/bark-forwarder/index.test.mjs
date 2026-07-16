import assert from 'node:assert/strict';
import test from 'node:test';

import { buildBarkRequest, parseIncomingEvent } from './index.mjs';

test('builds a redacted Bark POST request from an IoT event', () => {
  const event = parseIncomingEvent(JSON.stringify({
    event_type: 'high_temperature_critical',
    state: 'opened',
    severity: 'N3',
    timestamp_ms: 900000,
    display_c: 28.7,
    event_id: 'high_temperature_critical:opened:900000',
  }));
  const request = buildBarkRequest(event, {
    deviceKey: 'test-device-key',
    aquariumId: 'tank01',
  });

  assert.equal(request.url, 'https://api.day.app/push');
  assert.equal(request.init.method, 'POST');
  const body = JSON.parse(request.init.body);
  assert.equal(body.device_key, 'test-device-key');
  assert.equal(body.title, '鱼缸温度严重告警');
  assert.equal(body.group, 'aquarium');
  assert.equal(body.level, 'critical');
  assert.match(body.body, /high_temperature_critical/);
  assert.doesNotMatch(body.body, /test-device-key|password|DeviceSecret/i);
  assert.equal(request.fingerprint,
               'aquarium:tank01:high_temperature_critical:opened:900000');
});

test('rejects malformed incoming events before any delivery attempt', () => {
  assert.throws(() => parseIncomingEvent('{"event_type":"high_temperature"}'),
                /missing required field/);
});
