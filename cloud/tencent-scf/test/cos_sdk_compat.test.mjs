import assert from 'node:assert/strict';
import test from 'node:test';

import COS from 'cos-nodejs-sdk-v5';

test('installed COS SDK exposes the callback API used by the state store', () => {
  const client = new COS({ SecretId: 'test-id', SecretKey: 'test-key' });
  assert.equal(typeof client.getObject, 'function');
  assert.equal(typeof client.putObject, 'function');
});
