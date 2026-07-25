import { timingSafeEqual } from 'node:crypto';

const DEVICE_ID = 'tank01';
const STATE_PATH = `/api/v1/devices/${DEVICE_ID}/state`;
const EVENT_TYPES = new Set([
  'high_temperature',
  'high_temperature_critical',
  'low_temperature',
  'low_temperature_critical',
  'rapid_temperature_change',
  'temperature_rapid_change',
  'temperature_gradient',
  'sensor_fault',
  'network_offline',
  'network_recovered',
  'heartbeat_missing',
]);
const EVENT_STATES = new Set(['opened', 'escalated', 'reminder']);
const EVENT_SEVERITIES = new Set(['n2', 'n3']);

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

function requestMethod(event) {
  return event?.requestContext?.http?.method ?? event?.httpMethod ?? '';
}

function requestPath(event) {
  return event?.rawPath ?? event?.requestContext?.http?.path ?? event?.path ?? '';
}

function authorizationHeader(event) {
  const headers = event?.headers ?? {};
  return headers.authorization ?? headers.Authorization ?? '';
}

function matchesReadToken(value, readToken) {
  if (typeof value !== 'string' || !value.startsWith('Bearer ')) return false;
  const supplied = Buffer.from(value.slice('Bearer '.length), 'utf8');
  const expected = Buffer.from(readToken, 'utf8');
  return supplied.length === expected.length && timingSafeEqual(supplied, expected);
}

function isNullableTemperature(value) {
  return value === null || (typeof value === 'number' && Number.isFinite(value));
}

function projectEvent(event) {
  if (!event || typeof event !== 'object' || Array.isArray(event)) return null;
  if (!EVENT_TYPES.has(event.type) || !EVENT_STATES.has(event.state)
    || !EVENT_SEVERITIES.has(event.severity)
    || !Number.isSafeInteger(event.atMs) || event.atMs < 0
    || !isNullableTemperature(event.displayC)) {
    return null;
  }
  if (event.type === 'sensor_fault' && event.displayC !== null) return null;
  return {
    type: event.type,
    state: event.state,
    severity: event.severity,
    at_ms: event.atMs,
    display_c: event.displayC,
  };
}

function projectState(state) {
  if (!state || typeof state !== 'object' || state.schemaVersion !== 2
    || state.deviceId !== DEVICE_ID
    || !Number.isSafeInteger(state.lastSeenAtMs) || state.lastSeenAtMs < 0
    || !isNullableTemperature(state.mainC) || !isNullableTemperature(state.sumpC)
    || !['online', 'offline', 'unknown'].includes(state.connectivityStatus)
    || !Array.isArray(state.activeEvents) || state.activeEvents.length > 7) {
    return null;
  }

  const events = state.activeEvents.map(projectEvent);
  if (events.some((event) => event === null)) return null;
  if (new Set(events.map((event) => event.type)).size !== events.length) return null;

  return {
    schema_version: 1,
    device_id: DEVICE_ID,
    timestamp_ms: state.lastSeenAtMs,
    display_c: state.mainC,
    return_c: state.sumpC,
    connectivity_status: state.connectivityStatus,
    events,
  };
}

export function createStateReadHandler({ store, readToken }) {
  if (!store || typeof store.getDeviceState !== 'function') {
    throw new TypeError('A state store is required');
  }
  if (typeof readToken !== 'string' || readToken.length < 32 || readToken.length > 256) {
    throw new TypeError('A read token with 32 to 256 characters is required');
  }

  return async function stateReadHandler(event) {
    if (requestMethod(event) !== 'GET') {
      return jsonResponse(405, { ok: false, error: 'method_not_allowed' });
    }
    if (requestPath(event) !== STATE_PATH) {
      return jsonResponse(400, { ok: false, error: 'invalid_device_id' });
    }
    if (!matchesReadToken(authorizationHeader(event), readToken)) {
      return jsonResponse(401, { ok: false, error: 'unauthorized' });
    }

    try {
      const state = await store.getDeviceState(DEVICE_ID);
      if (state === null) {
        return jsonResponse(404, { ok: false, error: 'state_not_found' });
      }
      const projected = projectState(state);
      if (projected === null) {
        return jsonResponse(503, { ok: false, error: 'state_unavailable' });
      }
      return jsonResponse(200, projected);
    } catch {
      return jsonResponse(503, { ok: false, error: 'state_unavailable' });
    }
  };
}
