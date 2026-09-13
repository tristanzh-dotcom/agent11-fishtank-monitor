import { timingSafeEqual } from 'node:crypto';

const DEVICE_ID = 'tank01';
const STATE_PATH = `/api/v1/devices/${DEVICE_ID}/state`;
export const TEMPERATURE_SUMMARY_PATH = `/api/v1/devices/${DEVICE_ID}/temperature-summary`;
const TEMPERATURE_SUMMARY_MAX_AGE_MS = 10 * 60 * 1_000;
const MAX_TEMPERATURE_SUMMARY_BYTES = 768;
const SHANGHAI_OFFSET_MS = 8 * 60 * 60 * 1_000;
const NO_TEMPERATURE_DATA = '暂无温度数据，请稍后重试。';
const OFFLINE_TEMPERATURE_DATA = '设备已离线，温度信息暂不可用。';
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

function textResponse(statusCode, body) {
  return {
    statusCode,
    headers: {
      'content-type': 'text/plain; charset=utf-8',
      'cache-control': 'no-store',
    },
    body,
  };
}

function requestMethod(event) {
  return event?.requestContext?.http?.method ?? event?.httpMethod ?? '';
}

function requestPath(event) {
  const path = event?.rawPath ?? event?.requestContext?.http?.path ?? event?.path ?? '';
  return String(path).split('?')[0];
}

function authorizationHeader(event) {
  const headers = event?.headers ?? {};
  for (const [key, value] of Object.entries(headers)) {
    if (key.toLowerCase() === 'authorization') {
      return Array.isArray(value) ? value[0] ?? '' : value;
    }
  }
  return '';
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

function formatShanghaiTime(timestampMs) {
  const shifted = new Date(timestampMs + SHANGHAI_OFFSET_MS);
  if (!Number.isFinite(shifted.getTime())) return null;
  const pad = (value) => String(value).padStart(2, '0');
  return `${shifted.getUTCFullYear()}-${pad(shifted.getUTCMonth() + 1)}-${pad(shifted.getUTCDate())}`
    + ` ${pad(shifted.getUTCHours())}:${pad(shifted.getUTCMinutes())}:${pad(shifted.getUTCSeconds())}`
    + '（上海时间）';
}

function isWellFormedText(value) {
  for (let index = 0; index < value.length; index += 1) {
    const code = value.charCodeAt(index);
    if (code >= 0xd800 && code <= 0xdbff) {
      const next = value.charCodeAt(index + 1);
      if (Number.isNaN(next) || next < 0xdc00 || next > 0xdfff) return false;
      index += 1;
    } else if (code >= 0xdc00 && code <= 0xdfff) {
      return false;
    }
  }
  return true;
}

function readTemperatureSummary(state, nowMs) {
  if (state === null) return { kind: 'text', body: NO_TEMPERATURE_DATA };
  if (projectState(state) === null) return { kind: 'error' };
  if (state.connectivityStatus === 'offline') {
    return { kind: 'text', body: OFFLINE_TEMPERATURE_DATA };
  }

  const snapshot = state.temperatureSnapshot;
  if (snapshot === undefined) return { kind: 'text', body: NO_TEMPERATURE_DATA };
  if (!snapshot || typeof snapshot !== 'object' || Array.isArray(snapshot)
    || !Number.isSafeInteger(snapshot.sampledAtMs)
    || snapshot.sampledAtMs <= 0
    || typeof snapshot.summaryText !== 'string'
    || snapshot.summaryText.length === 0
    || !isWellFormedText(snapshot.summaryText)
    || Buffer.byteLength(snapshot.summaryText, 'utf8') > MAX_TEMPERATURE_SUMMARY_BYTES) {
    return { kind: 'error' };
  }
  if (snapshot.sampledAtMs > nowMs) {
    return { kind: 'text', body: NO_TEMPERATURE_DATA };
  }

  const ageMs = nowMs - snapshot.sampledAtMs;
  if (ageMs > TEMPERATURE_SUMMARY_MAX_AGE_MS) {
    const sampledAt = formatShanghaiTime(snapshot.sampledAtMs);
    if (sampledAt === null) return { kind: 'error' };
    return {
      kind: 'text',
      body: `温度数据暂未更新，最后采样时间：${sampledAt}`,
    };
  }
  return { kind: 'text', body: snapshot.summaryText };
}

export function createStateReadHandler({ store, readToken, clock = Date.now }) {
  if (!store || typeof store.getDeviceState !== 'function') {
    throw new TypeError('A state store is required');
  }
  if (typeof readToken !== 'string' || readToken.length < 32 || readToken.length > 256) {
    throw new TypeError('A read token with 32 to 256 characters is required');
  }

  return async function stateReadHandler(event) {
    const path = requestPath(event);
    if (requestMethod(event) !== 'GET') {
      if (path === TEMPERATURE_SUMMARY_PATH) {
        return textResponse(405, '仅允许 GET 请求。');
      }
      return jsonResponse(405, { ok: false, error: 'method_not_allowed' });
    }
    if (path !== STATE_PATH && path !== TEMPERATURE_SUMMARY_PATH) {
      return jsonResponse(400, { ok: false, error: 'invalid_device_id' });
    }
    if (!matchesReadToken(authorizationHeader(event), readToken)) {
      if (path === TEMPERATURE_SUMMARY_PATH) {
        return textResponse(401, '未授权。');
      }
      return jsonResponse(401, { ok: false, error: 'unauthorized' });
    }

    try {
      const state = await store.getDeviceState(DEVICE_ID);
      if (path === TEMPERATURE_SUMMARY_PATH) {
        const summary = readTemperatureSummary(state, clock());
        if (summary.kind === 'error') {
          return textResponse(503, '温度数据暂不可用。');
        }
        return textResponse(200, summary.body);
      }
      if (state === null) {
        return jsonResponse(404, { ok: false, error: 'state_not_found' });
      }
      const projected = projectState(state);
      if (projected === null) {
        return jsonResponse(503, { ok: false, error: 'state_unavailable' });
      }
      return jsonResponse(200, projected);
    } catch {
      if (path === TEMPERATURE_SUMMARY_PATH) {
        return textResponse(503, '温度数据暂不可用。');
      }
      return jsonResponse(503, { ok: false, error: 'state_unavailable' });
    }
  };
}
