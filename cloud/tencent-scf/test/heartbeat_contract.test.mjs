import assert from 'node:assert/strict';
import { createHmac, createHash } from 'node:crypto';
import test from 'node:test';

import {
  RequestError,
  authenticateHeartbeat,
} from '../src/heartbeat_contract.mjs';

const NOW_MS = 1_750_000_000_000;
const SECRET = 'test-secret-never-deploy';

function sign(body, timestamp, nonce) {
  const bodyHash = createHash('sha256').update(body).digest('hex');
  const canonical = `v1\n${timestamp}\n${nonce}\n${bodyHash}`;
  return createHmac('sha256', SECRET).update(canonical).digest('hex');
}

function makeEvent(overrides = {}) {
  const payload = overrides.payload ?? {
    device_id: 'tank01',
    sent_at_ms: NOW_MS - 1_000,
    nonce: '0123456789abcdef',
    main_c: 26.4,
    sump_c: 26.6,
    uptime_ms: 123_456,
  };
  const body = overrides.body ?? JSON.stringify(payload);
  const signature = overrides.signature ?? sign(
    body,
    payload.sent_at_ms,
    payload.nonce,
  );

  return {
    requestContext: { http: { method: overrides.method ?? 'POST' } },
    headers: {
      'content-type': 'application/json',
      'x-aquarium-signature': signature,
      ...overrides.headers,
    },
    body,
    isBase64Encoded: false,
  };
}

const options = {
  deviceSecrets: { tank01: SECRET },
  nowMs: NOW_MS,
};

test('accepts a valid signed heartbeat and normalizes its payload', () => {
  const result = authenticateHeartbeat(makeEvent(), options);

  assert.deepEqual(result.payload, {
    deviceId: 'tank01',
    sentAtMs: NOW_MS - 1_000,
    nonce: '0123456789abcdef',
    mainC: 26.4,
    sumpC: 26.6,
    uptimeMs: 123_456,
  });
  assert.equal(result.rawBody.length > 0, true);
});

test('rejects unsupported methods, oversized bodies, and stale timestamps', () => {
  assert.throws(
    () => authenticateHeartbeat(makeEvent({ method: 'GET' }), options),
    (error) => error instanceof RequestError && error.statusCode === 405,
  );

  const largePayload = {
    device_id: 'tank01',
    sent_at_ms: NOW_MS,
    nonce: '0123456789abcdef',
    main_c: 26,
    sump_c: 26,
    uptime_ms: 1,
    padding: 'x'.repeat(1_100),
  };
  assert.throws(
    () => authenticateHeartbeat(makeEvent({ payload: largePayload }), options),
    (error) => error instanceof RequestError && error.statusCode === 413,
  );

  const stalePayload = {
    device_id: 'tank01',
    sent_at_ms: NOW_MS - 300_001,
    nonce: '0123456789abcdef',
    main_c: 26,
    sump_c: 26,
    uptime_ms: 1,
  };
  assert.throws(
    () => authenticateHeartbeat(makeEvent({ payload: stalePayload }), options),
    (error) => error instanceof RequestError && error.statusCode === 401,
  );
});

test('rejects malformed JSON before authentication or storage', () => {
  assert.throws(
    () => authenticateHeartbeat(makeEvent({ body: '{bad json' }), options),
    (error) => error instanceof RequestError && error.statusCode === 400,
  );
});

test('rejects unknown devices, malformed fields, and invalid signatures', () => {
  const unknownPayload = {
    device_id: 'tank99',
    sent_at_ms: NOW_MS,
    nonce: '0123456789abcdef',
    main_c: 26,
    sump_c: 26,
    uptime_ms: 1,
  };
  assert.throws(
    () => authenticateHeartbeat(makeEvent({ payload: unknownPayload }), options),
    (error) => error instanceof RequestError && error.statusCode === 401,
  );

  const malformedPayload = {
    device_id: 'tank01',
    sent_at_ms: NOW_MS,
    nonce: 'not-hex',
    main_c: 99,
    sump_c: 26,
    uptime_ms: -1,
  };
  assert.throws(
    () => authenticateHeartbeat(makeEvent({ payload: malformedPayload }), options),
    (error) => error instanceof RequestError && error.statusCode === 400,
  );

  assert.throws(
    () => authenticateHeartbeat(makeEvent({ signature: '0'.repeat(64) }), options),
    (error) => error instanceof RequestError && error.statusCode === 401,
  );
});

test('supports base64 request bodies and case-insensitive headers', () => {
  const plainEvent = makeEvent();
  const event = {
    ...plainEvent,
    headers: {
      'Content-Type': 'application/json; charset=utf-8',
      'X-Aquarium-Signature': plainEvent.headers['x-aquarium-signature'],
    },
    body: Buffer.from(plainEvent.body).toString('base64'),
    isBase64Encoded: true,
  };

  const result = authenticateHeartbeat(event, options);
  assert.equal(result.payload.deviceId, 'tank01');
});

test('verifies an uppercase hex nonce exactly as transmitted, then normalizes it', () => {
  const event = makeEvent({
    payload: {
      device_id: 'tank01',
      sent_at_ms: NOW_MS,
      nonce: 'ABCDEF0123456789',
      main_c: 26,
      sump_c: 26,
      uptime_ms: 1,
    },
  });

  const result = authenticateHeartbeat(event, options);
  assert.equal(result.payload.nonce, 'abcdef0123456789');
});
