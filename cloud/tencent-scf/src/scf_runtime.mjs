import COS from 'cos-nodejs-sdk-v5';

import { createBarkNotifier } from './bark_notifier.mjs';
import { createCosStateStore } from './cos_state_store.mjs';
import { createHeartbeatHandler } from './heartbeat_handler.mjs';
import { createOfflineChecker } from './offline_checker.mjs';
import { readHeartbeatConfig, readOfflineConfig } from './runtime_config.mjs';

function credentialsFromContext(context) {
  const SecretId = context?.TENCENTCLOUD_SECRETID;
  const SecretKey = context?.TENCENTCLOUD_SECRETKEY;
  const SecurityToken = context?.TENCENTCLOUD_SESSIONTOKEN;
  if (!SecretId || !SecretKey || !SecurityToken) {
    throw new Error('SCF invocation credentials are unavailable');
  }
  return { SecretId, SecretKey, SecurityToken };
}

function isHttpEvent(event) {
  return Boolean(event?.requestContext?.http?.method ?? event?.httpMethod);
}

function createStore(config, context, CosCtor) {
  return createCosStateStore({
    cos: new CosCtor(credentialsFromContext(context)),
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
      store: createStore(config, context, CosCtor),
      deviceSecrets: config.deviceSecrets,
      minimumDurationMs: minimumHeartbeatDurationMs,
    })(event);
  }

  if (event?.Type === 'Timer') {
    const config = readOfflineConfig(env);
    return createOfflineChecker({
      store: createStore(config, context, CosCtor),
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
