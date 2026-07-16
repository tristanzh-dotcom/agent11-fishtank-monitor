import assert from 'node:assert/strict';
import test from 'node:test';

import {
  readHeartbeatConfig,
  readOfflineConfig,
} from '../src/runtime_config.mjs';

const base = {
  COS_BUCKET: 'fishtank-monitor-1454792551',
  COS_REGION: 'ap-shanghai',
};

test('reads strict heartbeat configuration without defaults for secrets', () => {
  const config = readHeartbeatConfig({
    ...base,
    DEVICE_SECRETS_JSON: JSON.stringify({
      tank01: '0123456789abcdef0123456789abcdef',
    }),
  });

  assert.equal(config.bucket, base.COS_BUCKET);
  assert.equal(config.region, 'ap-shanghai');
  assert.deepEqual(Object.keys(config.deviceSecrets), ['tank01']);
});

test('reads offline configuration for the single safe-mode device', () => {
  const config = readOfflineConfig({
    ...base,
    DEVICE_IDS: 'tank01',
    BARK_KEY: 'private-bark-key',
    OFFLINE_AFTER_MS: '900000',
  });

  assert.deepEqual(config.deviceIds, ['tank01']);
  assert.equal(config.offlineAfterMs, 900_000);
  assert.equal(config.barkKey, 'private-bark-key');

  assert.throws(
    () => readOfflineConfig({ ...base, DEVICE_IDS: 'tank01', BARK_KEY: 'key1', OFFLINE_AFTER_MS: '1' }),
    /OFFLINE_AFTER_MS/,
  );
});

test('rejects missing, malformed, or unsafe configuration', () => {
  assert.throws(
    () => readHeartbeatConfig({ ...base, DEVICE_SECRETS_JSON: '{}' }),
    /DEVICE_SECRETS_JSON/,
  );
  assert.throws(
    () => readHeartbeatConfig({
      ...base,
      DEVICE_SECRETS_JSON: '{bad json',
    }),
    /DEVICE_SECRETS_JSON/,
  );
  assert.throws(
    () => readHeartbeatConfig({
      ...base,
      DEVICE_SECRETS_JSON: JSON.stringify({ '../tank': '0123456789abcdef' }),
    }),
    /tank01/,
  );
  assert.throws(
    () => readOfflineConfig({
      ...base,
      DEVICE_IDS: 'tank01,tank02',
      BARK_KEY: 'private-bark-key',
    }),
    /tank01/,
  );
  assert.throws(
    () => readHeartbeatConfig({
      ...base,
      DEVICE_SECRETS_JSON: JSON.stringify({
        tank01: '0123456789abcdef',
        tank02: '0123456789abcdef',
      }),
    }),
    /tank01/,
  );
  assert.throws(
    () => readHeartbeatConfig({
      ...base,
      COS_BUCKET: 'wrong-bucket-1454792551',
      DEVICE_SECRETS_JSON: JSON.stringify({ tank01: '0123456789abcdef' }),
    }),
    /COS_BUCKET/,
  );
});
