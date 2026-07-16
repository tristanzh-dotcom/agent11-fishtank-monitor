'use strict';

const { createSerializedHandler } = require('./serializer.js');

function isHttpEvent(event) {
  return Boolean(event?.requestContext?.http?.method ?? event?.httpMethod);
}

function serviceUnavailable() {
  return {
    statusCode: 503,
    headers: {
      'content-type': 'application/json; charset=utf-8',
      'cache-control': 'no-store',
    },
    body: JSON.stringify({ ok: false, error: 'service_unavailable' }),
  };
}

async function run(event, context) {
  const startedAt = Date.now();
  const http = isHttpEvent(event);
  try {
    const { dispatchScfEvent } = await import('./src/scf_runtime.mjs');
    return await dispatchScfEvent(event, context, process.env, {
      minimumHeartbeatDurationMs: 0,
    });
  } catch {
    return http
      ? serviceUnavailable()
      : { checked: 0, transitioned: 0, failed: 1 };
  } finally {
    if (http) {
      const remaining = 500 - (Date.now() - startedAt);
      if (remaining > 0) {
        await new Promise((resolve) => setTimeout(resolve, remaining));
      }
    }
  }
}

exports.main_handler = createSerializedHandler(run);
