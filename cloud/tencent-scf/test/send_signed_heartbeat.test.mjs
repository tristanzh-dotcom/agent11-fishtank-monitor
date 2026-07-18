import assert from 'node:assert/strict';
import { createHash, createHmac } from 'node:crypto';
import test from 'node:test';

import {
  buildSignedHeartbeatRequest,
  runSignedHeartbeat,
} from '../scripts/send-signed-heartbeat.mjs';

const URL = 'https://example.test/heartbeat';
const SECRET = 'test-secret-never-deploy';
const NOW_MS = 1_750_000_000_000;
const NONCE = '00112233445566778899aabbccddeeff';

test('builds the exact JSON body and HMAC header', () => {
  const request = buildSignedHeartbeatRequest({
    url: URL,
    secret: SECRET,
    nowMs: NOW_MS,
    nonce: NONCE,
    mainC: 26.4,
    sumpC: 26.75,
    uptimeMs: 123_456,
  });

  assert.equal(request.url, URL);
  assert.equal(
    request.options.body,
    '{"device_id":"tank01","sent_at_ms":1750000000000,'
      + '"nonce":"00112233445566778899aabbccddeeff",'
      + '"main_c":26.4,"sump_c":26.75,"uptime_ms":123456}',
  );
  const hash = createHash('sha256').update(request.options.body).digest('hex');
  const canonical = `v1\n${NOW_MS}\n${NONCE}\n${hash}`;
  assert.equal(
    request.options.headers['X-Aquarium-Signature'],
    createHmac('sha256', SECRET).update(canonical).digest('hex'),
  );
});

test('requires HTTPS URL and a sufficiently long secret', () => {
  assert.throws(
    () => buildSignedHeartbeatRequest({
      url: 'http://example.test',
      secret: SECRET,
      nowMs: NOW_MS,
      nonce: NONCE,
      mainC: 26,
      sumpC: 26,
      uptimeMs: 1,
    }),
    /HTTPS/,
  );
  assert.throws(
    () => buildSignedHeartbeatRequest({
      url: URL,
      secret: 'too-short',
      nowMs: NOW_MS,
      nonce: NONCE,
      mainC: 26,
      sumpC: 26,
      uptimeMs: 1,
    }),
    /secret/,
  );
});

test('runner emits only a redacted status summary', async () => {
  const output = [];
  const fakeFetch = async () => ({ ok: true, status: 200 });

  const result = await runSignedHeartbeat({
    env: {
      FISHTANK_FUNCTION_URL: URL,
      FISHTANK_DEVICE_SECRET: SECRET,
    },
    fetchImpl: fakeFetch,
    writeLine: (line) => output.push(line),
    nowMs: NOW_MS,
    nonce: NONCE,
  });

  assert.deepEqual(result, { ok: true, status: 200 });
  assert.equal(output.join('\n').includes(SECRET), false);
  assert.equal(output.join('\n').includes(URL), false);
  assert.equal(output.join('\n').includes(NONCE), false);
  assert.match(output.join('\n'), /HTTP 200/);
});

