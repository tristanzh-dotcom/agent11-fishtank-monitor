import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import test from 'node:test';

const require = createRequire(import.meta.url);

test('exposes one serialized Tencent SCF handler for both triggers', () => {
  const monitor = require('../monitor.js');

  assert.equal(typeof monitor.main_handler, 'function');
});
