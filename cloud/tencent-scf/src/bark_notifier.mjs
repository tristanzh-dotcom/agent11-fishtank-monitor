function messageFor(alert) {
  if (alert.type === 'offline') {
    const minutes = Math.max(
      0,
      Math.floor((alert.detectedAtMs - alert.lastSeenAtMs) / 60_000),
    );
    return {
      title: `鱼缸监控离线：${alert.deviceId}`,
      body: `已连续 ${minutes} 分钟未收到心跳，请检查 Wi-Fi、ESP32 供电和鱼缸现场。`,
      level: 'timeSensitive',
    };
  }

  return {
    title: `鱼缸监控恢复：${alert.deviceId}`,
    body: '云端已重新收到设备心跳。',
    level: 'active',
  };
}

export function createBarkNotifier({
  barkKey,
  fetchImpl = fetch,
  serverUrl = 'https://api.day.app',
}) {
  if (typeof barkKey !== 'string' || barkKey.length < 4) {
    throw new TypeError('A Bark key is required');
  }

  return {
    async send(alert) {
      const message = messageFor(alert);
      const response = await fetchImpl(`${serverUrl.replace(/\/$/, '')}/push`, {
        method: 'POST',
        headers: { 'content-type': 'application/json' },
        body: JSON.stringify({
          device_key: barkKey,
          title: message.title,
          body: message.body,
          group: '鱼缸监控',
          level: message.level,
        }),
        signal: AbortSignal.timeout(2_000),
      });

      if (!response.ok) {
        throw new Error(`Bark request failed with status ${response.status}`);
      }
    },
  };
}
