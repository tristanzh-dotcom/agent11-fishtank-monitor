'use strict';

const { createSerializedHandler } = require('./serializer.js');

const TEMPERATURE_SUMMARY_PATH = '/api/v1/devices/esp1/temperature-summary';
const LEGACY_TEMPERATURE_SUMMARY_PATH = '/api/v1/devices/tank01/temperature-summary';

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

function summaryUnavailable() {
  return {
    statusCode: 503,
    headers: {
      'content-type': 'text/plain; charset=utf-8',
      'cache-control': 'no-store',
    },
    body: '温度数据暂不可用。',
  };
}

function requestPath(event) {
  const path = event?.rawPath ?? event?.requestContext?.http?.path ?? event?.path ?? '';
  return String(path).split('?')[0];
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
      ? ([TEMPERATURE_SUMMARY_PATH, LEGACY_TEMPERATURE_SUMMARY_PATH].includes(requestPath(event))
        ? summaryUnavailable()
        : serviceUnavailable())
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
