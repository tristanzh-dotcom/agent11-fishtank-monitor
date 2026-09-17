import assert from 'node:assert/strict';
import test from 'node:test';

import { createStateReadHandler } from '../src/state_read_handler.mjs';

const READ_TOKEN = '0123456789abcdef0123456789abcdef';
const NOW_MS = 1_750_000_000_000;
const SUMMARY_PATH = '/api/v1/devices/esp1/temperature-summary';

function request({
  method = 'GET',
  path = '/api/v1/devices/esp1/state',
  authorization = `Bearer ${READ_TOKEN}`,
  authorizationHeaderName = 'authorization',
} = {}) {
  return {
    requestContext: { http: { method, path } },
    rawPath: path,
    headers: authorization === null ? {} : { [authorizationHeaderName]: authorization },
  };
}

function state() {
  return {
    schemaVersion: 2,
    deviceId: 'esp1',
    lastSeenAtMs: 1_750_000_000_000,
    mainC: 26.4,
    sumpC: 26.6,
    connectivityStatus: 'online',
    activeEvents: [{
      type: 'high_temperature',
      state: 'opened',
      severity: 'n2',
      atMs: 1_749_999_900_000,
      displayC: 27.8,
    }],
  };
}

function storeWith(initial) {
  let reads = 0;
  let writes = 0;
  return {
    async getDeviceState() { reads += 1; return initial; },
    async saveDeviceState() { writes += 1; },
    inspect() { return { reads, writes }; },
  };
}

test('projects a v2 private state into FishTankStateV1', async () => {
  const store = storeWith(state());
  const response = await createStateReadHandler({ store, readToken: READ_TOKEN })(request());

  assert.equal(response.statusCode, 200);
  assert.deepEqual(JSON.parse(response.body), {
    schema_version: 1,
    device_id: 'esp1',
    timestamp_ms: 1_750_000_000_000,
    display_c: 26.4,
    return_c: 26.6,
    connectivity_status: 'online',
    events: [{
      type: 'high_temperature',
      state: 'opened',
      severity: 'n2',
      at_ms: 1_749_999_900_000,
      display_c: 27.8,
    }],
  });
  assert.deepEqual(store.inspect(), { reads: 1, writes: 0 });
  assert.equal(response.body.includes(READ_TOKEN), false);
});

test('keeps the legacy tank01 state path read-only while reading esp1 state', async () => {
  const store = storeWith({ ...state(), deviceId: 'esp1' });
  const response = await createStateReadHandler({ store, readToken: READ_TOKEN })(
    request({ path: '/api/v1/devices/tank01/state' }),
  );

  assert.equal(response.statusCode, 200);
  assert.equal(JSON.parse(response.body).device_id, 'esp1');
  assert.deepEqual(store.inspect(), { reads: 1, writes: 0 });
});

test('projects heartbeat contract temperature event types from a real v2 snapshot', async () => {
  const store = storeWith({
    ...state(),
    activeEvents: [{
      type: 'low_temperature_critical',
      state: 'opened',
      severity: 'n2',
      atMs: 1_749_999_900_000,
      displayC: 19.4,
    }],
  });
  const response = await createStateReadHandler({ store, readToken: READ_TOKEN })(request());

  assert.equal(response.statusCode, 200);
  assert.equal(JSON.parse(response.body).events[0].type, 'low_temperature_critical');
});

test('projects temperature gradient events from a real v2 snapshot', async () => {
  const store = {
    async getDeviceState() {
      return {
        schemaVersion: 2,
        deviceId: 'esp1',
        lastSeenAtMs: 1_700_000_000_000,
        mainC: 19.4,
        sumpC: 24.8,
        connectivityStatus: 'online',
        activeEvents: [{
          type: 'temperature_gradient',
          state: 'opened',
          severity: 'n2',
          atMs: 1_700_000_000_000,
          displayC: 19.4,
        }],
      };
    },
  };
  const handler = createStateReadHandler({ store, readToken: READ_TOKEN });
  const response = await handler(request());
  assert.equal(response.statusCode, 200);
  assert.equal(JSON.parse(response.body).events[0].type, 'temperature_gradient');
});

test('rejects missing or invalid read tokens without accessing COS', async () => {
  for (const authorization of [null, 'Bearer wrong-token']) {
    const store = storeWith(state());
    const response = await createStateReadHandler({ store, readToken: READ_TOKEN })(
      request({ authorization }),
    );
    assert.equal(response.statusCode, 401);
    assert.deepEqual(JSON.parse(response.body), { ok: false, error: 'unauthorized' });
    assert.deepEqual(store.inspect(), { reads: 0, writes: 0 });
  }
});

test('returns stable errors for missing, legacy, invalid-path, and write requests', async () => {
  const cases = [
    { storeState: null, requestOptions: {}, statusCode: 404, error: 'state_not_found' },
    { storeState: { ...state(), schemaVersion: 1 }, requestOptions: {}, statusCode: 503, error: 'state_unavailable' },
    { storeState: state(), requestOptions: { path: '/api/v1/devices/tank99/state' }, statusCode: 400, error: 'invalid_device_id' },
    { storeState: state(), requestOptions: { method: 'POST' }, statusCode: 405, error: 'method_not_allowed' },
  ];

  for (const item of cases) {
    const store = storeWith(item.storeState);
    const response = await createStateReadHandler({ store, readToken: READ_TOKEN })(
      request(item.requestOptions),
    );
    assert.equal(response.statusCode, item.statusCode);
    assert.deepEqual(JSON.parse(response.body), { ok: false, error: item.error });
    assert.equal(store.inspect().writes, 0);
  }
});

test('returns a fresh summary as plain text without writing state', async () => {
  const store = storeWith({
    ...state(),
    temperatureSnapshot: {
      sampledAtMs: NOW_MS - 120_000,
      summaryText: '采样时间：2025-06-15 23:04:40（上海时间）\n老四缸：25.0°C',
    },
  });
  const response = await createStateReadHandler({
    store,
    readToken: READ_TOKEN,
    clock: () => NOW_MS,
  })(request({ path: SUMMARY_PATH }));

  assert.equal(response.statusCode, 200);
  assert.equal(response.headers['content-type'], 'text/plain; charset=utf-8');
  assert.equal(response.headers['cache-control'], 'no-store');
  assert.equal(response.body, '采样时间：2025-06-15 23:04:40（上海时间）\n老四缸：25.0°C');
  assert.deepEqual(store.inspect(), { reads: 1, writes: 0 });
});

test('accepts mixed-case Authorization headers on the summary route', async () => {
  const store = storeWith({
    ...state(),
    temperatureSnapshot: {
      sampledAtMs: NOW_MS - 1_000,
      summaryText: '混合大小写请求头',
    },
  });
  const response = await createStateReadHandler({
    store,
    readToken: READ_TOKEN,
    clock: () => NOW_MS,
  })(request({ path: SUMMARY_PATH, authorizationHeaderName: 'AuThOrIzAtIoN' }));

  assert.equal(response.statusCode, 200);
  assert.equal(response.body, '混合大小写请求头');
});

test('returns no-data, offline, future, and stale messages without old temperatures', async () => {
  const cases = [
    {
      label: 'missing snapshot',
      value: state(),
      expected: '暂无温度数据，请稍后重试。',
    },
    {
      label: 'offline state',
      value: {
        ...state(),
        connectivityStatus: 'offline',
        temperatureSnapshot: {
          sampledAtMs: NOW_MS - 1_000,
          summaryText: '旧温度 26.4°C',
        },
      },
      expected: '设备已离线，温度信息暂不可用。',
    },
    {
      label: 'future sample',
      value: {
        ...state(),
        temperatureSnapshot: {
          sampledAtMs: NOW_MS + 1,
          summaryText: '未来温度 26.4°C',
        },
      },
      expected: '暂无温度数据，请稍后重试。',
    },
    {
      label: 'sample older than ten minutes',
      value: {
        ...state(),
        temperatureSnapshot: {
          sampledAtMs: NOW_MS - 600_001,
          summaryText: '旧温度 26.4°C',
        },
      },
      expected: '温度数据暂未更新，最后采样时间：2025-06-15 22:56:39（上海时间）',
    },
  ];

  for (const item of cases) {
    const store = storeWith(item.value);
    const response = await createStateReadHandler({
      store,
      readToken: READ_TOKEN,
      clock: () => NOW_MS,
    })(request({ path: SUMMARY_PATH }));
    assert.equal(response.statusCode, 200, item.label);
    assert.equal(response.body, item.expected, item.label);
    assert.equal(response.body.includes('26.4°C'), false, item.label);
    assert.equal(store.inspect().writes, 0, item.label);
  }
});

test('treats exactly ten minutes as fresh and rejects damaged summary state', async () => {
  const freshStore = storeWith({
    ...state(),
    temperatureSnapshot: {
      sampledAtMs: NOW_MS - 600_000,
      summaryText: '刚好十分钟',
    },
  });
  const freshResponse = await createStateReadHandler({
    store: freshStore,
    readToken: READ_TOKEN,
    clock: () => NOW_MS,
  })(request({ path: SUMMARY_PATH }));
  assert.equal(freshResponse.statusCode, 200);
  assert.equal(freshResponse.body, '刚好十分钟');

  const damagedStore = storeWith({
    ...state(),
    temperatureSnapshot: { sampledAtMs: NOW_MS, summaryText: 42 },
  });
  const damagedResponse = await createStateReadHandler({
    store: damagedStore,
    readToken: READ_TOKEN,
    clock: () => NOW_MS,
  })(request({ path: SUMMARY_PATH }));
  assert.equal(damagedResponse.statusCode, 503);
  assert.equal(damagedResponse.headers['content-type'], 'text/plain; charset=utf-8');
  assert.equal(damagedResponse.body, '温度数据暂不可用。');

  const failedStore = {
    async getDeviceState() {
      throw new Error('COS unavailable');
    },
  };
  const failedResponse = await createStateReadHandler({
    store: failedStore,
    readToken: READ_TOKEN,
    clock: () => NOW_MS,
  })(request({ path: SUMMARY_PATH }));
  assert.equal(failedResponse.statusCode, 503);
  assert.equal(failedResponse.headers['content-type'], 'text/plain; charset=utf-8');
  assert.equal(failedResponse.body, '温度数据暂不可用。');
});

test('rejects a summary when the stored private state is missing core readings', async () => {
  const store = storeWith({
    ...state(),
    mainC: undefined,
    sumpC: undefined,
    activeEvents: undefined,
    temperatureSnapshot: {
      sampledAtMs: NOW_MS - 1_000,
      summaryText: '不应返回的旧摘要',
    },
  });
  const response = await createStateReadHandler({
    store,
    readToken: READ_TOKEN,
    clock: () => NOW_MS,
  })(request({ path: SUMMARY_PATH }));

  assert.equal(response.statusCode, 503);
  assert.equal(response.body, '温度数据暂不可用。');
  assert.equal(response.body.includes('旧摘要'), false);
});

test('protects the summary route with the existing token and method boundary', async () => {
  for (const options of [
    { authorization: null, expectedStatus: 401, expectedBody: '未授权。' },
    { method: 'POST', expectedStatus: 405, expectedBody: '仅允许 GET 请求。' },
  ]) {
    const store = storeWith(state());
    const response = await createStateReadHandler({
      store,
      readToken: READ_TOKEN,
      clock: () => NOW_MS,
    })(request({ ...options, path: SUMMARY_PATH }));
    assert.equal(response.statusCode, options.expectedStatus);
    assert.equal(response.headers['content-type'], 'text/plain; charset=utf-8');
    assert.equal(response.body, options.expectedBody);
    assert.deepEqual(store.inspect(), { reads: 0, writes: 0 });
  }
});
