import assert from 'node:assert/strict';
import { createHash, createHmac } from 'node:crypto';
import test from 'node:test';

import { createHeartbeatHandler } from '../src/heartbeat_handler.mjs';

const NOW_MS = 1_750_000_000_000;
const SECRET = 'test-secret-never-deploy';

function eventFor(
  nonce = '0123456789abcdef',
  activeEvents = [],
  temperatureSnapshot,
) {
  const payload = {
    device_id: 'esp1',
    sent_at_ms: NOW_MS - 1_000,
    nonce,
    main_c: 26.4,
    sump_c: 26.6,
    uptime_ms: 123_456,
    active_events: activeEvents,
  };
  if (temperatureSnapshot !== undefined) {
    payload.temperature_snapshot = temperatureSnapshot;
  }
  const body = JSON.stringify(payload);
  const hash = createHash('sha256').update(body).digest('hex');
  const canonical = `v1\n${payload.sent_at_ms}\n${nonce}\n${hash}`;
  const signature = createHmac('sha256', SECRET).update(canonical).digest('hex');
  return {
    requestContext: { http: { method: 'POST' } },
    headers: {
      'content-type': 'application/json',
      'x-aquarium-signature': signature,
    },
    body,
  };
}

function memoryStore(initial = null) {
  let state = initial;
  let writes = 0;
  return {
    async getDeviceState() { return state; },
    async saveDeviceState(_deviceId, next) { state = next; writes += 1; },
    inspect() { return { state, writes }; },
  };
}

function handlerFor(store, sleeps) {
  return createHeartbeatHandler({
    store,
    deviceSecrets: { esp1: SECRET },
    clock: () => NOW_MS,
    sleep: async (milliseconds) => sleeps.push(milliseconds),
  });
}

function handlerWithNotifier(store, sleeps, notifier) {
  return createHeartbeatHandler({
    store,
    deviceSecrets: { esp1: SECRET },
    clock: () => NOW_MS,
    sleep: async (milliseconds) => sleeps.push(milliseconds),
    notifier,
  });
}

test('persists one bounded device state and returns no secret data', async () => {
  const store = memoryStore();
  const sleeps = [];
  const response = await handlerFor(store, sleeps)(eventFor());
  const body = JSON.parse(response.body);

  assert.equal(response.statusCode, 200);
  assert.deepEqual(body, {
    ok: true,
    device_id: 'esp1',
    last_seen_at_ms: NOW_MS,
  });
  assert.deepEqual(store.inspect(), {
    writes: 1,
    state: {
      schemaVersion: 2,
      deviceId: 'esp1',
      lastSeenAtMs: NOW_MS,
      sentAtMs: NOW_MS - 1_000,
      recentNonces: ['0123456789abcdef'],
      mainC: 26.4,
      sumpC: 26.6,
      uptimeMs: 123_456,
      activeEvents: [],
      connectivityStatus: 'online',
      recoveryPending: false,
    },
  });
  assert.deepEqual(sleeps, [500]);
  assert.equal(response.body.includes(SECRET), false);
});

test('rejects nonce replay without writing state', async () => {
  const store = memoryStore({
    deviceId: 'esp1',
    recentNonces: ['0123456789abcdef'],
    connectivityStatus: 'online',
  });
  const sleeps = [];
  const response = await handlerFor(store, sleeps)(eventFor());

  assert.equal(response.statusCode, 409);
  assert.deepEqual(JSON.parse(response.body), { ok: false, error: 'replay_detected' });
  assert.equal(store.inspect().writes, 0);
  assert.deepEqual(sleeps, [500]);
});

test('does not notify when a heartbeat nonce is replayed', async () => {
  const event = {
    type: 'high_temperature',
    state: 'opened',
    severity: 'n2',
    at_ms: NOW_MS,
    display_c: 28.1,
  };
  const store = memoryStore({
    recentNonces: ['aaaaaaaaaaaaaaaa'],
    activeEvents: [],
    connectivityStatus: 'online',
  });
  const sleeps = [];
  const alerts = [];
  const notifier = { async send(alert) { alerts.push(alert); } };

  const response = await handlerWithNotifier(store, sleeps, notifier)(
    eventFor('aaaaaaaaaaaaaaaa', [event]),
  );

  assert.equal(response.statusCode, 409);
  assert.deepEqual(alerts, []);
  assert.equal(store.inspect().writes, 0);
});

test('marks recovery pending after an offline state and caps nonce history', async () => {
  const oldNonces = Array.from({ length: 16 }, (_, index) => index.toString(16).padStart(16, '0'));
  const store = memoryStore({
    deviceId: 'esp1',
    recentNonces: oldNonces,
    connectivityStatus: 'offline',
  });
  const sleeps = [];
  const response = await handlerFor(store, sleeps)(eventFor('fedcba9876543210'));
  const state = store.inspect().state;

  assert.equal(response.statusCode, 200);
  assert.equal(state.connectivityStatus, 'online');
  assert.equal(state.recoveryPending, true);
  assert.equal(state.recentNonces.length, 16);
  assert.equal(state.recentNonces.at(-1), 'fedcba9876543210');
  assert.equal(state.recentNonces.includes(oldNonces[0]), false);
});

test('persists a valid temperature snapshot and clears it on an old heartbeat', async () => {
  const snapshot = {
    sampled_at_ms: NOW_MS - 2_000,
    summary_text: '采样时间：2025-06-15 23:06:38（上海时间）',
  };
  const store = memoryStore();
  const sleeps = [];
  const first = await handlerFor(store, sleeps)(
    eventFor('aaaaaaaaaaaaaaaa', [], snapshot),
  );

  assert.equal(first.statusCode, 200);
  assert.deepEqual(store.inspect().state.temperatureSnapshot, {
    sampledAtMs: NOW_MS - 2_000,
    summaryText: snapshot.summary_text,
  });

  const second = await handlerFor(store, sleeps)(
    eventFor('bbbbbbbbbbbbbbbb'),
  );
  assert.equal(second.statusCode, 200);
  assert.equal('temperatureSnapshot' in store.inspect().state, false);
});

test('notifies once for a newly opened temperature event before saving state', async () => {
  const store = memoryStore();
  const sleeps = [];
  const alerts = [];
  const notifier = { async send(alert) { alerts.push(alert); } };
  const event = {
    type: 'high_temperature',
    state: 'opened',
    severity: 'n2',
    at_ms: NOW_MS,
    display_c: 28.1,
  };

  const response = await handlerWithNotifier(store, sleeps, notifier)(
    eventFor('aaaaaaaaaaaaaaaa', [event]),
  );

  assert.equal(response.statusCode, 200);
  assert.deepEqual(alerts, [{
    type: 'temperature',
    deviceId: 'esp1',
    mainC: 26.4,
    sumpC: 26.6,
    event: {
      type: event.type,
      state: event.state,
      severity: event.severity,
      atMs: event.at_ms,
      displayC: event.display_c,
    },
  }]);
  assert.equal(store.inspect().writes, 1);
});

test('does not repeat an unchanged active event on the next heartbeat', async () => {
  const event = {
    type: 'high_temperature',
    state: 'opened',
    severity: 'n2',
    at_ms: NOW_MS,
    display_c: 28.1,
  };
  const store = memoryStore({
    schemaVersion: 2,
    deviceId: 'esp1',
    recentNonces: [],
    activeEvents: [event],
    connectivityStatus: 'online',
  });
  const sleeps = [];
  const alerts = [];
  const notifier = { async send(alert) { alerts.push(alert); } };

  const response = await handlerWithNotifier(store, sleeps, notifier)(
    eventFor('bbbbbbbbbbbbbbbb', [event]),
  );

  assert.equal(response.statusCode, 200);
  assert.deepEqual(alerts, []);
});

test('does not save state when a temperature alert cannot be delivered', async () => {
  const store = memoryStore();
  const sleeps = [];
  const notifier = { async send() { throw new Error('Bark unavailable'); } };
  const event = {
    type: 'temperature_rapid_change',
    state: 'escalated',
    severity: 'n3',
    at_ms: NOW_MS,
    display_c: 30.1,
  };

  const response = await handlerWithNotifier(store, sleeps, notifier)(
    eventFor('cccccccccccccccc', [event]),
  );

  assert.equal(response.statusCode, 503);
  assert.equal(store.inspect().writes, 0);
});

test('invalid requests do not read or write COS and still take at least 500ms', async () => {
  let reads = 0;
  let writes = 0;
  const store = {
    async getDeviceState() { reads += 1; },
    async saveDeviceState() { writes += 1; },
  };
  const sleeps = [];
  const response = await handlerFor(store, sleeps)({
    requestContext: { http: { method: 'GET' } },
    headers: {},
    body: '',
  });

  assert.equal(response.statusCode, 405);
  assert.deepEqual(JSON.parse(response.body), { ok: false, error: 'request_rejected' });
  assert.equal(reads, 0);
  assert.equal(writes, 0);
  assert.deepEqual(sleeps, [500]);
});
