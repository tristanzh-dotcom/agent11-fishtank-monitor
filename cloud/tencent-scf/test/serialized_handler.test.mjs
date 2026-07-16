import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import test from 'node:test';

const require = createRequire(import.meta.url);
const { createSerializedHandler } = require('../serializer.js');

test('serializes overlapping invocations and continues after a rejection', async () => {
  const events = [];
  let releaseFirst;
  const firstGate = new Promise((resolve) => { releaseFirst = resolve; });
  const handler = createSerializedHandler(async (value) => {
    events.push(`start:${value}`);
    if (value === 'first') await firstGate;
    events.push(`end:${value}`);
    if (value === 'fail') throw new Error('expected');
    return value;
  });

  const first = handler('first');
  const second = handler('second');
  await new Promise((resolve) => setImmediate(resolve));
  assert.deepEqual(events, ['start:first']);
  releaseFirst();
  assert.deepEqual(await Promise.all([first, second]), ['first', 'second']);
  assert.deepEqual(events, ['start:first', 'end:first', 'start:second', 'end:second']);

  await assert.rejects(() => handler('fail'), /expected/);
  assert.equal(await handler('after-fail'), 'after-fail');
});
