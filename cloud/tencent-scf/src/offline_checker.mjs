const DEFAULT_OFFLINE_AFTER_MS = 15 * 60 * 1_000;

export function evaluateOfflineTransition(
  state,
  nowMs,
  offlineAfterMs = DEFAULT_OFFLINE_AFTER_MS,
) {
  if (!state || !Number.isSafeInteger(state.lastSeenAtMs)) return null;

  const stale = nowMs - state.lastSeenAtMs >= offlineAfterMs;
  if (stale && state.connectivityStatus !== 'offline') {
    return {
      type: 'offline',
      nextState: {
        ...state,
        connectivityStatus: 'offline',
        recoveryPending: false,
      },
    };
  }

  if (state.connectivityStatus === 'online' && state.recoveryPending === true) {
    return {
      type: 'recovered',
      nextState: { ...state, recoveryPending: false },
    };
  }

  return null;
}

export function createOfflineChecker({
  store,
  notifier,
  deviceIds,
  clock = Date.now,
  offlineAfterMs = DEFAULT_OFFLINE_AFTER_MS,
}) {
  if (!store || !notifier) throw new TypeError('Store and notifier are required');
  if (!Array.isArray(deviceIds) || deviceIds.length === 0) {
    throw new TypeError('At least one device id is required');
  }

  return async function offlineChecker() {
    const summary = { checked: 0, transitioned: 0, failed: 0 };

    for (const deviceId of deviceIds) {
      summary.checked += 1;
      try {
        const nowMs = clock();
        const state = await store.getDeviceState(deviceId);
        const transition = evaluateOfflineTransition(state, nowMs, offlineAfterMs);
        if (!transition) continue;

        await notifier.send({
          type: transition.type,
          deviceId,
          lastSeenAtMs: state.lastSeenAtMs,
          detectedAtMs: nowMs,
        });
        await store.saveDeviceState(deviceId, transition.nextState);
        summary.transitioned += 1;
      } catch {
        summary.failed += 1;
      }
    }

    return summary;
  };
}
