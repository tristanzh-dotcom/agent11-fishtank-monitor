import assert from 'node:assert/strict';
import test from 'node:test';

import { dispatchScfEvent } from '../src/scf_runtime.mjs';

const baseEnv = {
  COS_BUCKET: 'fishtank-monitor-1454792551',
  COS_REGION: 'ap-shanghai',
  DEVICE_SECRETS_JSON: JSON.stringify({ tank01: '0123456789abcdef' }),
};

function envWithRoleCredentials(secretId) {
  return {
    ...baseEnv,
    TENCENTCLOUD_SECRETID: secretId,
    TENCENTCLOUD_SECRETKEY: `${secretId}-key`,
    TENCENTCLOUD_SESSIONTOKEN: `${secretId}-token`,
  };
}

test('creates a fresh COS client from current invocation environment credentials', async () => {
  const credentials = [];
  class FakeCos {
    constructor(value) { credentials.push(value); }
    getObject() { throw new Error('invalid request must not read COS'); }
    putObject() { throw new Error('invalid request must not write COS'); }
  }
  const event = {
    requestContext: { http: { method: 'GET' } },
    headers: {},
    body: '',
  };

  await dispatchScfEvent(event, {}, envWithRoleCredentials('first-id'), {
    CosCtor: FakeCos,
    minimumHeartbeatDurationMs: 0,
  });
  await dispatchScfEvent(event, {}, envWithRoleCredentials('second-id'), {
    CosCtor: FakeCos,
    minimumHeartbeatDurationMs: 0,
  });

  assert.deepEqual(credentials, [
    { SecretId: 'first-id', SecretKey: 'first-id-key', SecurityToken: 'first-id-token' },
    { SecretId: 'second-id', SecretKey: 'second-id-key', SecurityToken: 'second-id-token' },
  ]);
});
