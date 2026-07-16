const DEVICE_ID_PATTERN = /^[a-z0-9][a-z0-9_-]{0,31}$/;

function requireDeviceId(deviceId) {
  if (typeof deviceId !== 'string' || !DEVICE_ID_PATTERN.test(deviceId)) {
    throw new TypeError('Invalid device id');
  }
}

function callCos(cos, method, params) {
  return new Promise((resolve, reject) => {
    cos[method](params, (error, result) => {
      if (error) reject(error);
      else resolve(result);
    });
  });
}

function isMissingObject(error) {
  return error?.code === 'NoSuchKey'
    || error?.code === 'NoSuchResource'
    || error?.statusCode === 404;
}

export function createCosStateStore({
  cos,
  bucket,
  region,
  prefix = 'devices',
}) {
  if (!cos || typeof cos.getObject !== 'function') {
    throw new TypeError('A COS client is required');
  }
  if (typeof bucket !== 'string' || bucket.length === 0) {
    throw new TypeError('COS bucket is required');
  }
  if (typeof region !== 'string' || region.length === 0) {
    throw new TypeError('COS region is required');
  }

  function keyFor(deviceId) {
    requireDeviceId(deviceId);
    return `${prefix}/${deviceId}/state.json`;
  }

  return {
    async getDeviceState(deviceId) {
      try {
        const result = await callCos(cos, 'getObject', {
          Bucket: bucket,
          Region: region,
          Key: keyFor(deviceId),
        });
        return JSON.parse(Buffer.from(result.Body).toString('utf8'));
      } catch (error) {
        if (isMissingObject(error)) return null;
        throw error;
      }
    },

    async saveDeviceState(deviceId, state) {
      await callCos(cos, 'putObject', {
        Bucket: bucket,
        Region: region,
        Key: keyFor(deviceId),
        Body: JSON.stringify(state),
        ContentType: 'application/json; charset=utf-8',
      });
    },
  };
}

