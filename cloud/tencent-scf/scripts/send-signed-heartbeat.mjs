import {
  createHash,
  createHmac,
  randomBytes,
} from 'node:crypto';
import { pathToFileURL } from 'node:url';

const NONCE_PATTERN = /^[a-f0-9]{16,64}$/;

function requireHttpsUrl(value) {
  let parsed;
  try {
    parsed = new URL(value);
  } catch {
    throw new Error('Function URL is invalid');
  }
  if (parsed.protocol !== 'https:') {
    throw new Error('Function URL must use HTTPS');
  }
  return parsed.toString();
}

function requireSecret(value) {
  if (typeof value !== 'string' || value.length < 16 || value.length > 256) {
    throw new Error('Device secret must contain 16 to 256 characters');
  }
  return value;
}

function requireFiniteNumber(value, name) {
  if (typeof value !== 'number' || !Number.isFinite(value)) {
    throw new Error(`${name} must be a finite number`);
  }
  return value;
}

export function buildSignedHeartbeatRequest({
  url,
  secret,
  deviceId = 'tank01',
  nowMs = Date.now(),
  nonce = randomBytes(16).toString('hex'),
  mainC = 26.4,
  sumpC = 26.6,
  uptimeMs = 0,
}) {
  const safeUrl = requireHttpsUrl(url);
  const safeSecret = requireSecret(secret);
  if (!NONCE_PATTERN.test(nonce)) {
    throw new Error('Nonce must be 16 to 64 lowercase hexadecimal characters');
  }
  if (!Number.isSafeInteger(nowMs) || nowMs < 0) {
    throw new Error('Timestamp must be a non-negative safe integer');
  }
  if (!Number.isSafeInteger(uptimeMs) || uptimeMs < 0) {
    throw new Error('Uptime must be a non-negative safe integer');
  }

  const body = JSON.stringify({
    device_id: deviceId,
    sent_at_ms: nowMs,
    nonce,
    main_c: requireFiniteNumber(mainC, 'Main temperature'),
    sump_c: requireFiniteNumber(sumpC, 'Sump temperature'),
    uptime_ms: uptimeMs,
  });
  const bodyHash = createHash('sha256').update(body).digest('hex');
  const canonical = `v1\n${nowMs}\n${nonce}\n${bodyHash}`;
  const signature = createHmac('sha256', safeSecret)
    .update(canonical)
    .digest('hex');

  return {
    url: safeUrl,
    options: {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'X-Aquarium-Signature': signature,
      },
      body,
    },
  };
}

export async function runSignedHeartbeat({
  env = process.env,
  fetchImpl = globalThis.fetch,
  writeLine = (line) => process.stdout.write(`${line}\n`),
  nowMs = Date.now(),
  nonce,
} = {}) {
  const request = buildSignedHeartbeatRequest({
    url: env.FISHTANK_FUNCTION_URL,
    secret: env.FISHTANK_DEVICE_SECRET,
    nowMs,
    nonce,
    mainC: Number(env.FISHTANK_MAIN_C ?? '26.4'),
    sumpC: Number(env.FISHTANK_SUMP_C ?? '26.6'),
    uptimeMs: Number(env.FISHTANK_UPTIME_MS ?? '0'),
  });
  const response = await fetchImpl(request.url, request.options);
  const result = { ok: response.ok, status: response.status };
  writeLine(
    `Synthetic heartbeat ${response.ok ? 'accepted' : 'rejected'} `
      + `(HTTP ${response.status})`,
  );
  return result;
}

if (import.meta.url === pathToFileURL(process.argv[1] ?? '').href) {
  try {
    const result = await runSignedHeartbeat();
    if (!result.ok) {
      process.exitCode = 1;
    }
  } catch {
    process.stderr.write('Synthetic heartbeat request failed safely\n');
    process.exitCode = 1;
  }
}

