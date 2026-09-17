const DEVICE_ID_PATTERN = /^[a-z0-9][a-z0-9_-]{0,31}$/;
const MIN_OFFLINE_AFTER_MS = 5 * 60 * 1_000;
const MAX_OFFLINE_AFTER_MS = 24 * 60 * 60 * 1_000;
const SAFE_MODE_BUCKET = 'fishtank-monitor-1454792551';
const SAFE_MODE_DEVICE_ID = 'esp1';

function readCosConfig(env) {
  const bucket = env.COS_BUCKET;
  const region = env.COS_REGION;
  if (bucket !== SAFE_MODE_BUCKET) {
    throw new Error(`COS_BUCKET must be ${SAFE_MODE_BUCKET}`);
  }
  if (region !== 'ap-shanghai') {
    throw new Error('COS_REGION must be ap-shanghai');
  }
  return { bucket, region };
}

function requireDeviceId(deviceId) {
  if (!DEVICE_ID_PATTERN.test(deviceId)) {
    throw new Error(`Invalid device id: ${deviceId}`);
  }
}

export function readHeartbeatConfig(env = process.env) {
  const cos = readCosConfig(env);
  let parsed;
  try {
    parsed = JSON.parse(env.DEVICE_SECRETS_JSON ?? '');
  } catch {
    throw new Error('DEVICE_SECRETS_JSON must be valid JSON');
  }

  if (!parsed || typeof parsed !== 'object' || Array.isArray(parsed)) {
    throw new Error('DEVICE_SECRETS_JSON must be an object');
  }
  const entries = Object.entries(parsed);
  if (entries.length !== 1 || entries[0][0] !== SAFE_MODE_DEVICE_ID) {
    throw new Error('DEVICE_SECRETS_JSON must contain only esp1 in safe mode');
  }
  for (const [deviceId, secret] of entries) {
    requireDeviceId(deviceId);
    if (typeof secret !== 'string' || secret.length < 16 || secret.length > 256) {
      throw new Error('DEVICE_SECRETS_JSON contains an invalid secret');
    }
  }

  return { ...cos, deviceSecrets: parsed };
}

export function readStateApiConfig(env = process.env) {
  const cos = readCosConfig(env);
  const readToken = env.STATE_READ_TOKEN;
  if (typeof readToken !== 'string' || readToken.length < 32 || readToken.length > 256) {
    throw new Error('STATE_READ_TOKEN must contain 32 to 256 characters');
  }
  return { ...cos, readToken };
}

export function readOfflineConfig(env = process.env) {
  const cos = readCosConfig(env);
  const deviceIds = [...new Set(
    String(env.DEVICE_IDS ?? '')
      .split(',')
      .map((value) => value.trim())
      .filter(Boolean),
  )];
  if (deviceIds.length !== 1 || deviceIds[0] !== SAFE_MODE_DEVICE_ID) {
    throw new Error('DEVICE_IDS must be exactly esp1 in safe mode');
  }
  deviceIds.forEach(requireDeviceId);

  const barkKey = env.BARK_KEY;
  if (typeof barkKey !== 'string' || barkKey.length < 4 || barkKey.length > 256) {
    throw new Error('BARK_KEY is required');
  }

  const offlineAfterMs = Number(env.OFFLINE_AFTER_MS ?? 900_000);
  if (
    !Number.isSafeInteger(offlineAfterMs)
    || offlineAfterMs < MIN_OFFLINE_AFTER_MS
    || offlineAfterMs > MAX_OFFLINE_AFTER_MS
  ) {
    throw new Error('OFFLINE_AFTER_MS is outside the allowed range');
  }

  return { ...cos, deviceIds, barkKey, offlineAfterMs };
}
