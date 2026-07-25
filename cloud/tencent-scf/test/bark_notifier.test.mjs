import assert from 'node:assert/strict';
import test from 'node:test';

import { createBarkNotifier } from '../src/bark_notifier.mjs';

test('formats a temperature event as a Bark alert without putting the key in the URL', async () => {
  let request;
  const notifier = createBarkNotifier({
    barkKey: 'bark-key-not-in-url',
    serverUrl: 'https://bark.example.test',
    fetchImpl: async (url, options) => {
      request = { url, options, body: JSON.parse(options.body) };
      return { ok: true };
    },
  });

  await notifier.send({
    type: 'temperature',
    deviceId: 'tank01',
    mainC: 28.1,
    sumpC: 27.8,
    event: {
      type: 'high_temperature',
      state: 'opened',
      severity: 'n2',
      atMs: 1_750_000_000_000,
      displayC: 28.1,
    },
  });

  assert.equal(request.url, 'https://bark.example.test/push');
  assert.equal(request.body.device_key, 'bark-key-not-in-url');
  assert.equal(request.body.title, '高温告警｜包包大缸・主缸');
  assert.match(request.body.body, /主缸：28\.1°C/);
  assert.match(request.body.body, /底滤缸：27\.8°C/);
  assert.match(request.body.body, /阈值\/条件：27\.5°C/);
  assert.equal(request.options.method, 'POST');
});

test('uses user-facing Chinese templates for low temperature and sensor faults', async () => {
  const messages = [];
  const notifier = createBarkNotifier({
    barkKey: 'bark-key-not-in-url',
    serverUrl: 'https://bark.example.test',
    fetchImpl: async (_url, options) => {
      messages.push(JSON.parse(options.body));
      return { ok: true };
    },
  });

  await notifier.send({
    type: 'temperature',
    deviceId: 'tank01',
    mainC: 23.1,
    sumpC: 23.4,
    event: {
      type: 'low_temperature',
      state: 'opened',
      severity: 'n2',
      atMs: 1_750_000_000_000,
      displayC: 23.1,
    },
  });
  await notifier.send({
    type: 'temperature',
    deviceId: 'tank01',
    mainC: null,
    sumpC: 23.4,
    event: {
      type: 'sensor_fault',
      state: 'opened',
      severity: 'n2',
      atMs: 1_750_000_000_000,
      displayC: null,
    },
  });

  assert.equal(messages[0].title, '低温告警｜包包大缸・主缸');
  assert.match(messages[0].body, /阈值\/条件：23\.5°C/);
  assert.match(messages[0].body, /检查加热设备/);
  assert.equal(messages[1].title, '传感器故障｜包包大缸・主缸');
  assert.match(messages[1].body, /无有效读数/);
  assert.match(messages[1].body, /检查探头、接线和防水接头/);
});

test('uses the configured fish-tank display name for offline alerts', async () => {
  let body;
  const notifier = createBarkNotifier({
    barkKey: 'bark-key-not-in-url',
    serverUrl: 'https://bark.example.test',
    fetchImpl: async (_url, options) => {
      body = JSON.parse(options.body);
      return { ok: true };
    },
  });

  await notifier.send({
    type: 'offline',
    deviceId: 'tank01',
    lastSeenAtMs: 1_750_000_000_000,
    detectedAtMs: 1_750_000_900_000,
  });

  assert.equal(body.title, '设备离线｜包包大缸');
});
