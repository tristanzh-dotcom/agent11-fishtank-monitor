import {
  RequestError,
  authenticateHeartbeat,
} from './heartbeat_contract.mjs';

const MAX_RECENT_NONCES = 16;

class ReplayError extends Error {}

function jsonResponse(statusCode, body) {
  return {
    statusCode,
    headers: {
      'content-type': 'application/json; charset=utf-8',
      'cache-control': 'no-store',
    },
    body: JSON.stringify(body),
  };
}

function nextState(previous, payload, nowMs) {
  const recentNonces = Array.isArray(previous?.recentNonces)
    ? previous.recentNonces.filter((value) => typeof value === 'string')
    : [];
  if (recentNonces.includes(payload.nonce)) {
    throw new ReplayError('nonce already used');
  }

  return {
    schemaVersion: 2,
    deviceId: payload.deviceId,
    lastSeenAtMs: nowMs,
    sentAtMs: payload.sentAtMs,
    recentNonces: [...recentNonces, payload.nonce].slice(-MAX_RECENT_NONCES),
    mainC: payload.mainC,
    sumpC: payload.sumpC,
    uptimeMs: payload.uptimeMs,
    activeEvents: payload.activeEvents,
    ...(payload.temperatureSnapshot === undefined
      ? {}
      : { temperatureSnapshot: payload.temperatureSnapshot }),
    connectivityStatus: 'online',
    recoveryPending: previous?.connectivityStatus === 'offline'
      || previous?.recoveryPending === true,
  };
}

export function createHeartbeatHandler({
  store,
  deviceSecrets,
  clock = Date.now,
  sleep = (milliseconds) => new Promise((resolve) => setTimeout(resolve, milliseconds)),
  minimumDurationMs = 500,
}) {
  if (!store) throw new TypeError('A state store is required');

  return async function heartbeatHandler(event) {
    const startedAt = clock();
    let response;

    try {
      const authenticated = authenticateHeartbeat(event, {
        deviceSecrets,
        nowMs: startedAt,
      });
      const { payload } = authenticated;
      const previous = await store.getDeviceState(payload.deviceId);
      const state = nextState(previous, payload, startedAt);
      await store.saveDeviceState(payload.deviceId, state);

      response = jsonResponse(200, {
        ok: true,
        device_id: payload.deviceId,
        last_seen_at_ms: startedAt,
      });
    } catch (error) {
      if (error instanceof ReplayError) {
        response = jsonResponse(409, { ok: false, error: 'replay_detected' });
      } else if (error instanceof RequestError) {
        response = jsonResponse(error.statusCode, { ok: false, error: 'request_rejected' });
      } else {
        response = jsonResponse(503, { ok: false, error: 'service_unavailable' });
      }
    }

    const remaining = minimumDurationMs - (clock() - startedAt);
    if (remaining > 0) await sleep(remaining);
    return response;
  };
}
