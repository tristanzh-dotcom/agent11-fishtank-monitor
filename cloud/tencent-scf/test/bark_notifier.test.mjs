import assert from 'node:assert/strict';
import test from 'node:test';

import { createBarkNotifier } from '../src/bark_notifier.mjs';

test('formats a temperature event as a Bark alert without putting the key in the URL', async () => {
  let request;
  const notifier = createBarkNotifier({
    barkKey: 'bark-key-not-in-url',
    serverUrl: 'https://bark.example.test',
    clock: () => 1_750_000_001_000,
    fetchImpl: async (url, options) => {
      request = { url, options, body: JSON.parse(options.body) };
      return { ok: true };
    },
  });

  await notifier.send({
    type: 'temperature',
    deviceId: 'esp1',
    mainC: 27.3,
    sumpC: 26.7,
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
  assert.equal(request.body.title, '【注意·首次】包包缸·主缸｜高温');
  assert.match(request.body.body, /设备：腾讯云/);
  assert.match(request.body.body, /情况：当时水温：28\.1°C/);
  assert.match(request.body.body, /时间：判定 事件时间不可用；发送：2025-06-15 23:06:41/);
  assert.match(request.body.body, /次数：本问题第 1 次告警/);
  assert.match(request.body.body, /建议：核查最新水温及加热棒温控。/);
  assert.equal(request.body.body.includes('27.3°C'), false);
  assert.equal(request.body.body.includes('26.7°C'), false);
  assert.equal(request.options.method, 'POST');
});

test('uses user-facing Chinese templates for low temperature and sensor faults', async () => {
  const messages = [];
  const notifier = createBarkNotifier({
    barkKey: 'bark-key-not-in-url',
    serverUrl: 'https://bark.example.test',
    clock: () => 1_750_000_001_000,
    fetchImpl: async (_url, options) => {
      messages.push(JSON.parse(options.body));
      return { ok: true };
    },
  });

  await notifier.send({
    type: 'temperature',
    deviceId: 'esp1',
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
    deviceId: 'esp1',
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

  assert.equal(messages[0].title, '【注意·首次】包包缸·主缸｜低温');
  assert.match(messages[0].body, /情况：当时水温：23\.1°C/);
  assert.match(messages[0].body, /建议：核查最新水温及加热设备。/);
  assert.equal(messages[1].title, '【注意·首次】包包缸·主缸｜温度探头故障');
  assert.match(messages[1].body, /情况：当时水温：无有效读数/);
  assert.match(messages[1].body, /时间：判定 事件时间不可用/);
  assert.match(messages[1].body, /检查探头、接线和防水接头/);
});

test('uses state-specific wording for a cloud escalation', async () => {
  let body;
  const notifier = createBarkNotifier({
    barkKey: 'bark-key-not-in-url',
    serverUrl: 'https://bark.example.test',
    clock: () => 1_750_000_001_000,
    fetchImpl: async (_url, options) => {
      body = JSON.parse(options.body);
      return { ok: true };
    },
  });

  await notifier.send({
    type: 'temperature',
    deviceId: 'esp1',
    mainC: 26.0,
    sumpC: 26.0,
    event: {
      type: 'temperature_rapid_change',
      state: 'escalated',
      severity: 'n3',
      atMs: 1_750_000_000_000,
      displayC: 30.1,
    },
  });

  assert.equal(body.title, '【严重·升级】包包缸·主缸｜严重温度变化');
  assert.match(body.body, /时间：判定 事件时间不可用/);
});

test('reads a fresh cloud send time for each retry', async () => {
  const sentTimes = [1_750_000_000_000, 1_750_000_002_000];
  const bodies = [];
  const notifier = createBarkNotifier({
    barkKey: 'bark-key-not-in-url',
    serverUrl: 'https://bark.example.test',
    clock: () => sentTimes.shift(),
    fetchImpl: async (_url, options) => {
      bodies.push(JSON.parse(options.body));
      return { ok: true };
    },
  });
  const alert = {
    type: 'temperature',
    deviceId: 'esp1',
    event: {
      type: 'high_temperature',
      state: 'opened',
      severity: 'n2',
      atMs: 123,
      displayC: 28.1,
    },
  };

  await notifier.send(alert);
  await notifier.send(alert);

  assert.match(bodies[0].body, /发送：2025-06-15 23:06:40/);
  assert.match(bodies[1].body, /发送：2025-06-15 23:06:42/);
  assert.match(bodies[0].body, /时间：判定 事件时间不可用/);
  assert.match(bodies[1].body, /时间：判定 事件时间不可用/);
});

test('uses the configured device name and common count label for connectivity alerts', async () => {
  const bodies = [];
  const notifier = createBarkNotifier({
    barkKey: 'bark-key-not-in-url',
    serverUrl: 'https://bark.example.test',
    fetchImpl: async (_url, options) => {
      bodies.push(JSON.parse(options.body));
      return { ok: true };
    },
  });

  await notifier.send({
    type: 'offline',
    deviceId: 'esp1',
    lastSeenAtMs: 1_750_000_000_000,
    detectedAtMs: 1_750_000_900_000,
  });
  await notifier.send({
    type: 'recovered',
    deviceId: 'esp1',
    lastSeenAtMs: 1_750_000_000_000,
    detectedAtMs: 1_750_000_900_000,
  });

  assert.equal(bodies[0].title, '【注意·首次】鱼缸监控｜温控ESP1号连接中断');
  assert.match(bodies[0].body, /设备：温控ESP1号（云端监测）/);
  assert.match(bodies[0].body, /提醒次数：本次离线首次提醒/);
  assert.equal(bodies[1].title, '【信息·恢复】鱼缸监控｜温控ESP1号连接已恢复');
  assert.match(bodies[1].body, /设备：温控ESP1号（云端监测）/);
  assert.match(bodies[1].body, /提醒次数：不计入告警次数/);
});
