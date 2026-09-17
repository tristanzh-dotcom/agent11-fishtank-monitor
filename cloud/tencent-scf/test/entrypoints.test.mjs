import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import test from 'node:test';

const require = createRequire(import.meta.url);

test('exposes one serialized Tencent SCF handler for both triggers', () => {
  const monitor = require('../monitor.js');

  assert.equal(typeof monitor.main_handler, 'function');
});

test('redacts initialization failures and keeps the public 500ms response floor', async () => {
  const monitor = require('../monitor.js');
  const startedAt = Date.now();
  const response = await monitor.main_handler({
    requestContext: { http: { method: 'GET' } },
    headers: {},
    body: '',
  }, {});

  assert.equal(response.statusCode, 503);
  assert.deepEqual(JSON.parse(response.body), {
    ok: false,
    error: 'service_unavailable',
  });
  assert.equal(Date.now() - startedAt >= 480, true);
});

test('returns the summary route initialization failure as Chinese plain text', async () => {
  const monitor = require('../monitor.js');
  const response = await monitor.main_handler({
    requestContext: {
      http: {
        method: 'GET',
        path: '/api/v1/devices/esp1/temperature-summary',
      },
    },
    rawPath: '/api/v1/devices/esp1/temperature-summary',
    headers: {},
    body: '',
  }, {});

  assert.equal(response.statusCode, 503);
  assert.equal(response.headers['content-type'], 'text/plain; charset=utf-8');
  assert.equal(response.body, '温度数据暂不可用。');
});
