import COS from 'cos-nodejs-sdk-v5';

import { createBarkNotifier } from './bark_notifier.mjs';
import { createCosStateStore } from './cos_state_store.mjs';
import { createHeartbeatHandler } from './heartbeat_handler.mjs';
import { createOfflineChecker } from './offline_checker.mjs';
import { readHeartbeatConfig, readOfflineConfig } from './runtime_config.mjs';

function credentialsFromEnvironment(env) {
  const SecretId = env?.TENCENTCLOUD_SECRETID;
  const SecretKey = env?.TENCENTCLOUD_SECRETKEY;
  const SecurityToken = env?.TENCENTCLOUD_SESSIONTOKEN;
  if (!SecretId || !SecretKey || !SecurityToken) {
    throw new Error('SCF invocation credentials are unavailable');
  }
  return { SecretId, SecretKey, SecurityToken };
}

function isHttpEvent(event) {
  return Boolean(event?.requestContext?.http?.method ?? event?.httpMethod);
}

function createStore(config, env, CosCtor) {
  return createCosStateStore({
    cos: new CosCtor(credentialsFromEnvironment(env)),
    bucket: config.bucket,
    region: config.region,
  });
}

export async function dispatchScfEvent(
  event,
  context,
  env = process.env,
  {
    CosCtor = COS,
    fetchImpl = fetch,
    minimumHeartbeatDurationMs = 500,
  } = {},
) {
  if (isHttpEvent(event)) {
    const config = readHeartbeatConfig(env);
    return createHeartbeatHandler({
      store: createStore(config, env, CosCtor),
      deviceSecrets: config.deviceSecrets,
      minimumDurationMs: minimumHeartbeatDurationMs,
    })(event);
  }

  if (event?.Type === 'Timer') {
    const config = readOfflineConfig(env);
    return createOfflineChecker({
      store: createStore(config, env, CosCtor),
      notifier: createBarkNotifier({
        barkKey: config.barkKey,
        fetchImpl,
      }),
      deviceIds: config.deviceIds,
      offlineAfterMs: config.offlineAfterMs,
    })();
  }

  return { ok: false, error: 'unsupported_event' };
}
