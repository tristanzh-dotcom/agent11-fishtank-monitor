import assert from 'node:assert/strict';
import test from 'node:test';

import {
  createOfflineChecker,
  evaluateOfflineTransition,
} from '../src/offline_checker.mjs';

const NOW_MS = 1_750_000_000_000;
const FIFTEEN_MINUTES = 15 * 60 * 1_000;

test('offline transition fires once after 15 minutes', () => {
  const state = {
    deviceId: 'tank01',
    lastSeenAtMs: NOW_MS - FIFTEEN_MINUTES,
    connectivityStatus: 'online',
    recoveryPending: false,
    temperatureSnapshot: {
      sampledAtMs: NOW_MS - 60_000,
      summaryText: '温度摘要',
    },
  };

  const transition = evaluateOfflineTransition(state, NOW_MS, FIFTEEN_MINUTES);
  assert.equal(transition.type, 'offline');
  assert.equal(transition.nextState.connectivityStatus, 'offline');
  assert.equal(transition.nextState.recoveryPending, false);
  assert.deepEqual(transition.nextState.temperatureSnapshot,
                   state.temperatureSnapshot);

  assert.equal(
    evaluateOfflineTransition(transition.nextState, NOW_MS + 60_000, FIFTEEN_MINUTES),
    null,
  );
});

test('recovery transition clears the pending marker and missing state is ignored', () => {
  assert.equal(evaluateOfflineTransition(null, NOW_MS, FIFTEEN_MINUTES), null);

  const state = {
    deviceId: 'tank01',
    lastSeenAtMs: NOW_MS,
    connectivityStatus: 'online',
    recoveryPending: true,
  };
  const transition = evaluateOfflineTransition(state, NOW_MS, FIFTEEN_MINUTES);
  assert.equal(transition.type, 'recovered');
  assert.equal(transition.nextState.recoveryPending, false);
});

test('does not emit recovery when the recovery heartbeat is already stale again', () => {
  const state = {
    deviceId: 'tank01',
    lastSeenAtMs: NOW_MS - FIFTEEN_MINUTES,
    connectivityStatus: 'online',
    recoveryPending: true,
  };

  const transition = evaluateOfflineTransition(state, NOW_MS, FIFTEEN_MINUTES);
  assert.equal(transition.type, 'offline');
  assert.equal(transition.nextState.recoveryPending, false);
});

test('checker notifies before persisting so failed notifications retry later', async () => {
  const initial = {
    deviceId: 'tank01',
    lastSeenAtMs: NOW_MS - FIFTEEN_MINUTES - 1,
    connectivityStatus: 'online',
    recoveryPending: false,
  };
  let state = initial;
  let writes = 0;
  const store = {
    async getDeviceState() { return state; },
    async saveDeviceState(_deviceId, value) { state = value; writes += 1; },
  };
  const failedChecker = createOfflineChecker({
    store,
    notifier: { async send() { throw new Error('network down'); } },
    deviceIds: ['tank01'],
    clock: () => NOW_MS,
  });

  const failed = await failedChecker();
  assert.deepEqual(failed, { checked: 1, transitioned: 0, failed: 1 });
  assert.equal(writes, 0);
  assert.equal(state.connectivityStatus, 'online');

  const alerts = [];
  const successfulChecker = createOfflineChecker({
    store,
    notifier: { async send(alert) { alerts.push(alert); } },
    deviceIds: ['tank01'],
    clock: () => NOW_MS,
  });
  const succeeded = await successfulChecker();

  assert.deepEqual(succeeded, { checked: 1, transitioned: 1, failed: 0 });
  assert.equal(writes, 1);
  assert.equal(state.connectivityStatus, 'offline');
  assert.deepEqual(alerts, [{
    type: 'offline',
    deviceId: 'tank01',
    lastSeenAtMs: initial.lastSeenAtMs,
    detectedAtMs: NOW_MS,
  }]);
});
