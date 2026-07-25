const DISPLAY_NAME = '包包大缸';

function messageFor(alert) {
  if (alert.type === 'temperature') {
    const event = alert.event;
    const triggered = event.displayC === null ? '缺测' : `${event.displayC}°C`;
    const main = alert.mainC === null ? '缺测' : `${alert.mainC}°C`;
    const sump = alert.sumpC === null ? '缺测' : `${alert.sumpC}°C`;
    const templates = {
      high_temperature: {
        title: '高温告警', threshold: '27.5°C', action: '检查环境温度和加热棒温控。',
      },
      high_temperature_critical: {
        title: '严重高温', threshold: '28.5°C', action: '请立即检查加热棒和温控状态。',
      },
      low_temperature: {
        title: '低温告警', threshold: '23.5°C', action: '检查加热设备和环境温度。',
      },
      low_temperature_critical: {
        title: '严重低温', threshold: '22.5°C', action: '请立即检查加热设备和环境温度。',
      },
      temperature_rapid_change: {
        title: '温度快速变化', threshold: '30 分钟窗口', action: '检查加热设备和探头位置。',
      },
      temperature_gradient: {
        title: '缸体温差异常', threshold: '温差持续超限', action: '检查水流循环和探头位置。',
      },
      sensor_fault: {
        title: '传感器故障', threshold: '无有效读数', action: '请检查探头、接线和防水接头。',
      },
    };
    const template = templates[event.type] ?? {
      title: '温度告警', threshold: '超出监控条件', action: '请检查鱼缸监控设备。',
    };
    const title = event.severity === 'n3' && !template.title.startsWith('严重')
      ? `严重${template.title}`
      : template.title;
    const condition = event.type === 'sensor_fault'
      ? `主缸：${alert.mainC === null ? '无有效读数' : main}\n底滤缸：${sump}`
      : `主缸：${main}\n底滤缸：${sump}\n阈值/条件：${template.threshold}\n触发读数：${triggered}`;
    return {
      title: `${title}｜${DISPLAY_NAME}・主缸`,
      body: `${condition}\n\n建议：${template.action}`,
      level: event.severity === 'n3' ? 'timeSensitive' : 'active',
    };
  }
  if (alert.type === 'offline') {
    const minutes = Math.max(
      0,
      Math.floor((alert.detectedAtMs - alert.lastSeenAtMs) / 60_000),
    );
    return {
      title: `设备离线｜${DISPLAY_NAME}`,
      body: `已连续 ${minutes} 分钟未收到心跳，请检查 Wi-Fi、ESP32 供电和鱼缸现场。`,
      level: 'timeSensitive',
    };
  }

  return {
    title: `设备恢复｜${DISPLAY_NAME}`,
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
