import assert from 'node:assert/strict';
import test from 'node:test';

import { createCosStateStore } from '../src/cos_state_store.mjs';

function fakeCos() {
  const calls = [];
  return {
    calls,
    getObject(params, callback) {
      calls.push(['getObject', params]);
      callback(null, { Body: Buffer.from('{"deviceId":"tank01"}') });
    },
    putObject(params, callback) {
      calls.push(['putObject', params]);
      callback(null, { ETag: 'etag' });
    },
  };
}

test('reads and overwrites only the fixed per-device object key', async () => {
  const cos = fakeCos();
  const store = createCosStateStore({
    cos,
    bucket: 'fishtank-monitor-1454792551',
    region: 'ap-shanghai',
  });

  assert.deepEqual(await store.getDeviceState('tank01'), { deviceId: 'tank01' });
  await store.saveDeviceState('tank01', { deviceId: 'tank01', mainC: 26.4 });

  assert.equal(cos.calls[0][1].Key, 'devices/tank01/state.json');
  assert.equal(cos.calls[1][1].Key, 'devices/tank01/state.json');
  assert.equal(cos.calls[1][1].ContentType, 'application/json; charset=utf-8');
  assert.equal(cos.calls[1][1].Body, '{"deviceId":"tank01","mainC":26.4}');
});

test('treats a missing state object as null and rejects invalid device ids', async () => {
  const cos = {
    getObject(_params, callback) {
      const error = new Error('missing');
      error.code = 'NoSuchKey';
      callback(error);
    },
  };
  const store = createCosStateStore({
    cos,
    bucket: 'fishtank-monitor-1454792551',
    region: 'ap-shanghai',
  });

  assert.equal(await store.getDeviceState('tank01'), null);
  await assert.rejects(() => store.getDeviceState('../escape'), /device id/i);
});
