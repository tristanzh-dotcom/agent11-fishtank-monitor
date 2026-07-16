'use strict';

exports.createSerializedHandler = (handler) => {
  let tail = Promise.resolve();
  return (...args) => {
    const result = tail.then(() => handler(...args));
    tail = result.catch(() => undefined);
    return result;
  };
};
