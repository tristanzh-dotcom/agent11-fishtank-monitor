import assert from 'node:assert/strict';
import test from 'node:test';

import { createStateReadHandler } from '../src/state_read_handler.mjs';

const READ_TOKEN = '0123456789abcdef0123456789abcdef';

function request({
  method = 'GET',
  path = '/api/v1/devices/tank01/state',
  authorization = `Bearer ${READ_TOKEN}`,
} = {}) {
  return {
    requestContext: { http: { method, path } },
    rawPath: path,
    headers: authorization === null ? {} : { authorization },
  };
}

function state() {
  return {
    schemaVersion: 2,
    deviceId: 'tank01',
    lastSeenAtMs: 1_750_000_000_000,
    mainC: 26.4,
    sumpC: 26.6,
    connectivityStatus: 'online',
    activeEvents: [{
      type: 'high_temperature',
      state: 'opened',
      severity: 'n2',
      atMs: 1_749_999_900_000,
      displayC: 27.8,
    }],
  };
}

function storeWith(initial) {
  let reads = 0;
  let writes = 0;
  return {
    async getDeviceState() { reads += 1; return initial; },
    async saveDeviceState() { writes += 1; },
    inspect() { return { reads, writes }; },
  };
}

test('projects a v2 private state into FishTankStateV1', async () => {
  const store = storeWith(state());
  const response = await createStateReadHandler({ store, readToken: READ_TOKEN })(request());

  assert.equal(response.statusCode, 200);
  assert.deepEqual(JSON.parse(response.body), {
    schema_version: 1,
    device_id: 'tank01',
    timestamp_ms: 1_750_000_000_000,
    display_c: 26.4,
    return_c: 26.6,
    connectivity_status: 'online',
    events: [{
      type: 'high_temperature',
      state: 'opened',
      severity: 'n2',
      at_ms: 1_749_999_900_000,
      display_c: 27.8,
    }],
  });
  assert.deepEqual(store.inspect(), { reads: 1, writes: 0 });
  assert.equal(response.body.includes(READ_TOKEN), false);
});

test('projects heartbeat contract temperature event types from a real v2 snapshot', async () => {
  const store = storeWith({
    ...state(),
    activeEvents: [{
      type: 'low_temperature_critical',
      state: 'opened',
      severity: 'n2',
      atMs: 1_749_999_900_000,
      displayC: 19.4,
    }],
  });
  const response = await createStateReadHandler({ store, readToken: READ_TOKEN })(request());

  assert.equal(response.statusCode, 200);
  assert.equal(JSON.parse(response.body).events[0].type, 'low_temperature_critical');
});

test('projects temperature gradient events from a real v2 snapshot', async () => {
  const store = {
    async getDeviceState() {
      return {
        schemaVersion: 2,
        deviceId: 'tank01',
        lastSeenAtMs: 1_700_000_000_000,
        mainC: 19.4,
        sumpC: 24.8,
        connectivityStatus: 'online',
        activeEvents: [{
          type: 'temperature_gradient',
          state: 'opened',
          severity: 'n2',
          atMs: 1_700_000_000_000,
          displayC: 19.4,
        }],
      };
    },
  };
  const handler = createStateReadHandler({ store, readToken: READ_TOKEN });
  const response = await handler(request());
  assert.equal(response.statusCode, 200);
  assert.equal(JSON.parse(response.body).events[0].type, 'temperature_gradient');
});

test('rejects missing or invalid read tokens without accessing COS', async () => {
  for (const authorization of [null, 'Bearer wrong-token']) {
    const store = storeWith(state());
    const response = await createStateReadHandler({ store, readToken: READ_TOKEN })(
      request({ authorization }),
    );
    assert.equal(response.statusCode, 401);
    assert.deepEqual(JSON.parse(response.body), { ok: false, error: 'unauthorized' });
    assert.deepEqual(store.inspect(), { reads: 0, writes: 0 });
  }
});

test('returns stable errors for missing, legacy, invalid-path, and write requests', async () => {
  const cases = [
    { storeState: null, requestOptions: {}, statusCode: 404, error: 'state_not_found' },
    { storeState: { ...state(), schemaVersion: 1 }, requestOptions: {}, statusCode: 503, error: 'state_unavailable' },
    { storeState: state(), requestOptions: { path: '/api/v1/devices/tank99/state' }, statusCode: 400, error: 'invalid_device_id' },
    { storeState: state(), requestOptions: { method: 'POST' }, statusCode: 405, error: 'method_not_allowed' },
  ];

  for (const item of cases) {
    const store = storeWith(item.storeState);
    const response = await createStateReadHandler({ store, readToken: READ_TOKEN })(
      request(item.requestOptions),
    );
    assert.equal(response.statusCode, item.statusCode);
    assert.deepEqual(JSON.parse(response.body), { ok: false, error: item.error });
    assert.equal(store.inspect().writes, 0);
  }
});
