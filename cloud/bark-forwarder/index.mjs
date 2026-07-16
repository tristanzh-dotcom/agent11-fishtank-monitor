const BARK_PUSH_URL = 'https://api.day.app/push';
const REQUIRED_FIELDS = ['event_type', 'state', 'severity', 'timestamp_ms', 'display_c', 'event_id'];

function assertEventShape(candidate) {
  if (!candidate || typeof candidate !== 'object' || Array.isArray(candidate)) {
    throw new Error('incoming event must be an object');
  }
  for (const field of REQUIRED_FIELDS) {
    if (candidate[field] === undefined || candidate[field] === null) {
      throw new Error(`missing required field: ${field}`);
    }
  }
  if (!['N2', 'N3'].includes(candidate.severity)) {
    throw new Error('severity must be N2 or N3');
  }
  return candidate;
}

export function parseIncomingEvent(event) {
  if (Buffer.isBuffer(event)) {
    return parseIncomingEvent(event.toString('utf8'));
  }
  if (typeof event === 'string') {
    try {
      return assertEventShape(JSON.parse(event));
    } catch (error) {
      if (error instanceof SyntaxError) {
        throw new Error('incoming event is not valid JSON');
      }
      throw error;
    }
  }
  return assertEventShape(event);
}

function barkContent(event) {
  return `事件=${event.event_type} 状态=${event.state} 水温=${Number(event.display_c).toFixed(2)}C`;
}

export function buildBarkRequest(event, { deviceKey, aquariumId }) {
  if (!deviceKey) {
    throw new Error('BARK_DEVICE_KEY is required');
  }
  const normalized = assertEventShape(event);
  const critical = normalized.severity === 'N3';
  const fingerprint = `aquarium:${aquariumId}:${normalized.event_id}`;
  return {
    url: BARK_PUSH_URL,
    fingerprint,
    init: {
      method: 'POST',
      headers: { 'content-type': 'application/json' },
      body: JSON.stringify({
        device_key: deviceKey,
        title: critical ? '鱼缸温度严重告警' : '鱼缸温度告警',
        body: barkContent(normalized),
        group: 'aquarium',
        level: critical ? 'critical' : 'timeSensitive',
      }),
    },
  };
}

export async function handler(event, _context) {
  const incoming = parseIncomingEvent(event);
  const request = buildBarkRequest(incoming, {
    deviceKey: process.env.BARK_DEVICE_KEY,
    aquariumId: process.env.AQUARIUM_ID || 'tank01',
  });
  const response = await fetch(request.url, request.init);
  if (!response.ok) {
    throw new Error(`Bark delivery failed with HTTP ${response.status}`);
  }
  return {
    delivered: true,
    fingerprint: request.fingerprint,
    event_type: incoming.event_type,
    state: incoming.state,
  };
}
