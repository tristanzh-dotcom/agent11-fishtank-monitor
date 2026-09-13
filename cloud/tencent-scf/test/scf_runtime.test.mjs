import assert from 'node:assert/strict';
import { createHash, createHmac } from 'node:crypto';
import test from 'node:test';

import { dispatchScfEvent } from '../src/scf_runtime.mjs';

const baseEnv = {
  COS_BUCKET: 'fishtank-monitor-1454792551',
  COS_REGION: 'ap-shanghai',
  DEVICE_SECRETS_JSON: JSON.stringify({ tank01: '0123456789abcdef' }),
  STATE_READ_TOKEN: '0123456789abcdef0123456789abcdef',
};
const DEVICE_SECRET = '0123456789abcdef';
const SUMMARY_PATH = '/api/v1/devices/tank01/temperature-summary';

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

test('does not route POST on the temperature summary path into heartbeat storage', async () => {
  const sentAtMs = Date.now();
  const payload = {
    device_id: 'tank01',
    sent_at_ms: sentAtMs,
    nonce: '0123456789abcdef',
    main_c: 26.4,
    sump_c: 26.6,
    uptime_ms: 123,
    active_events: [],
  };
  const body = JSON.stringify(payload);
  const bodyHash = createHash('sha256').update(body).digest('hex');
  const signature = createHmac('sha256', DEVICE_SECRET)
    .update(`v1\n${sentAtMs}\n${payload.nonce}\n${bodyHash}`)
    .digest('hex');
  let reads = 0;
  let writes = 0;
  class FakeCos {
    getObject(_params, callback) {
      reads += 1;
      callback(null, { Body: Buffer.from('{}') });
    }
    putObject(_params, callback) {
      writes += 1;
      callback(null, {});
    }
  }

  for (const path of [SUMMARY_PATH, `${SUMMARY_PATH}?ignored=1`]) {
    const response = await dispatchScfEvent({
      requestContext: { http: { method: 'POST', path } },
      rawPath: path,
      headers: {
        'content-type': 'application/json',
        'x-aquarium-signature': signature,
      },
      body,
    }, {}, envWithRoleCredentials('summary-path'), {
      CosCtor: FakeCos,
      minimumHeartbeatDurationMs: 0,
    });

    assert.equal(response.statusCode, 405);
    assert.equal(response.headers['content-type'], 'text/plain; charset=utf-8');
    assert.equal(response.body, '仅允许 GET 请求。');
  }
  assert.equal(reads, 0);
  assert.equal(writes, 0);
});
