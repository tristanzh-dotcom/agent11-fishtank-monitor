import assert from 'node:assert/strict';
import test from 'node:test';

import { createBarkNotifier } from '../src/bark_notifier.mjs';

test('sends Bark through JSON POST without placing the key in the URL', async () => {
  const calls = [];
  const notifier = createBarkNotifier({
    barkKey: 'private-test-key',
    fetchImpl: async (url, options) => {
      calls.push({ url, options });
      return { ok: true, status: 200 };
    },
  });

  await notifier.send({
    type: 'offline',
    deviceId: 'tank01',
    lastSeenAtMs: 1_750_000_000_000,
    detectedAtMs: 1_750_000_900_000,
  });

  assert.equal(calls[0].url, 'https://api.day.app/push');
  assert.equal(calls[0].url.includes('private-test-key'), false);
  assert.equal(calls[0].options.method, 'POST');
  const body = JSON.parse(calls[0].options.body);
  assert.equal(body.device_key, 'private-test-key');
  assert.equal(body.title.includes('离线'), true);
  assert.equal(body.group, '鱼缸监控');
});

test('throws on Bark rejection so state is not marked notified', async () => {
  const notifier = createBarkNotifier({
    barkKey: 'private-test-key',
    fetchImpl: async () => ({ ok: false, status: 500 }),
  });

  await assert.rejects(
    () => notifier.send({
      type: 'recovered',
      deviceId: 'tank01',
      lastSeenAtMs: 1_750_000_000_000,
      detectedAtMs: 1_750_000_000_000,
    }),
    /Bark request failed/,
  );
});
