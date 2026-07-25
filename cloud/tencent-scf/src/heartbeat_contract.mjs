import {
  createHash,
  createHmac,
  timingSafeEqual,
} from 'node:crypto';

const DEFAULT_MAX_BODY_BYTES = 2_048;
const DEFAULT_MAX_CLOCK_SKEW_MS = 300_000;
const DEVICE_ID_PATTERN = /^[a-z0-9][a-z0-9_-]{0,31}$/;
const NONCE_PATTERN = /^[a-fA-F0-9]{16,64}$/;
const SIGNATURE_PATTERN = /^[a-fA-F0-9]{64}$/;
const MAX_ACTIVE_EVENTS = 7;
const EVENT_TYPES = new Set([
  'high_temperature',
  'high_temperature_critical',
  'low_temperature',
  'low_temperature_critical',
  'sensor_fault',
  'temperature_rapid_change',
  'temperature_gradient',
]);
const EVENT_STATES = new Set(['opened', 'escalated', 'reminder']);
const EVENT_SEVERITIES = new Set(['n2', 'n3']);

export class RequestError extends Error {
  constructor(statusCode, message) {
    super(message);
    this.name = 'RequestError';
    this.statusCode = statusCode;
  }
}

function getMethod(event) {
  return event?.requestContext?.http?.method ?? event?.httpMethod;
}

function normalizeHeaders(headers = {}) {
  return Object.fromEntries(
    Object.entries(headers).map(([key, value]) => [
      key.toLowerCase(),
      Array.isArray(value) ? value[0] : value,
    ]),
  );
}

function decodeBody(event) {
  if (typeof event?.body !== 'string') {
    throw new RequestError(400, 'Request body must be a string');
  }

  try {
    return event.isBase64Encoded
      ? Buffer.from(event.body, 'base64').toString('utf8')
      : event.body;
  } catch {
    throw new RequestError(400, 'Request body encoding is invalid');
  }
}

function requireFiniteNumber(value, name, minimum, maximum) {
  if (
    typeof value !== 'number'
    || !Number.isFinite(value)
    || value < minimum
    || value > maximum
  ) {
    throw new RequestError(400, `${name} is invalid`);
  }
  return value;
}

function requireNullableTemperature(value, name) {
  if (value === null) return null;
  return requireFiniteNumber(value, name, -20, 60);
}

function normalizeActiveEvents(value) {
  if (!Array.isArray(value) || value.length > MAX_ACTIVE_EVENTS) {
    throw new RequestError(400, 'active_events is invalid');
  }

  const seenTypes = new Set();
  return value.map((event) => {
    if (!event || typeof event !== 'object' || Array.isArray(event)) {
      throw new RequestError(400, 'active_events is invalid');
    }
    if (!EVENT_TYPES.has(event.type) || seenTypes.has(event.type)) {
      throw new RequestError(400, 'active_events is invalid');
    }
    if (!EVENT_STATES.has(event.state) || !EVENT_SEVERITIES.has(event.severity)) {
      throw new RequestError(400, 'active_events is invalid');
    }
    if (!Number.isSafeInteger(event.at_ms) || event.at_ms < 0) {
      throw new RequestError(400, 'active_events is invalid');
    }
    const displayC = requireNullableTemperature(event.display_c, 'active_events');
    if (event.type === 'sensor_fault' && displayC !== null) {
      throw new RequestError(400, 'active_events is invalid');
    }
    seenTypes.add(event.type);
    return {
      type: event.type,
      state: event.state,
      severity: event.severity,
      atMs: event.at_ms,
      displayC,
    };
  });
}

function normalizePayload(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value)) {
    throw new RequestError(400, 'JSON body must be an object');
  }

  const deviceId = value.device_id;
  if (typeof deviceId !== 'string' || !DEVICE_ID_PATTERN.test(deviceId)) {
    throw new RequestError(400, 'device_id is invalid');
  }
  if (!Number.isSafeInteger(value.sent_at_ms) || value.sent_at_ms < 0) {
    throw new RequestError(400, 'sent_at_ms is invalid');
  }
  if (typeof value.nonce !== 'string' || !NONCE_PATTERN.test(value.nonce)) {
    throw new RequestError(400, 'nonce is invalid');
  }
  if (!Number.isSafeInteger(value.uptime_ms) || value.uptime_ms < 0) {
    throw new RequestError(400, 'uptime_ms is invalid');
  }

  return {
    deviceId,
    sentAtMs: value.sent_at_ms,
    nonce: value.nonce,
    mainC: requireNullableTemperature(value.main_c, 'main_c'),
    sumpC: requireNullableTemperature(value.sump_c, 'sump_c'),
    uptimeMs: value.uptime_ms,
    activeEvents: normalizeActiveEvents(value.active_events),
  };
}

function verifySignature(rawBody, payload, signature, secret) {
  if (typeof signature !== 'string' || !SIGNATURE_PATTERN.test(signature)) {
    throw new RequestError(401, 'Authentication failed');
  }

  const bodyHash = createHash('sha256').update(rawBody).digest('hex');
  const canonical = `v1\n${payload.sentAtMs}\n${payload.nonce}\n${bodyHash}`;
  const expected = createHmac('sha256', secret).update(canonical).digest();
  const received = Buffer.from(signature, 'hex');

  if (received.length !== expected.length || !timingSafeEqual(received, expected)) {
    throw new RequestError(401, 'Authentication failed');
  }
}

export function authenticateHeartbeat(event, {
  deviceSecrets,
  nowMs = Date.now(),
  maxBodyBytes = DEFAULT_MAX_BODY_BYTES,
  maxClockSkewMs = DEFAULT_MAX_CLOCK_SKEW_MS,
} = {}) {
  if (getMethod(event) !== 'POST') {
    throw new RequestError(405, 'Only POST is allowed');
  }

  const headers = normalizeHeaders(event?.headers);
  if (!String(headers['content-type'] ?? '').toLowerCase().startsWith('application/json')) {
    throw new RequestError(415, 'Content-Type must be application/json');
  }

  const rawBody = decodeBody(event);
  if (Buffer.byteLength(rawBody, 'utf8') > maxBodyBytes) {
    throw new RequestError(413, 'Request body is too large');
  }

  let parsed;
  try {
    parsed = JSON.parse(rawBody);
  } catch {
    throw new RequestError(400, 'Request body is not valid JSON');
  }

  const payload = normalizePayload(parsed);
  const secret = deviceSecrets?.[payload.deviceId];
  if (typeof secret !== 'string' || secret.length < 16) {
    throw new RequestError(401, 'Authentication failed');
  }
  if (Math.abs(nowMs - payload.sentAtMs) > maxClockSkewMs) {
    throw new RequestError(401, 'Heartbeat timestamp is outside the allowed window');
  }

  verifySignature(
    rawBody,
    payload,
    headers['x-aquarium-signature'],
    secret,
  );

  return {
    payload: { ...payload, nonce: payload.nonce.toLowerCase() },
    rawBody,
  };
}
