const DISPLAY_NAME = '包包大缸';
const TEMPERATURE_DISPLAY_NAME = '包包缸';

const SHANGHAI_OFFSET_MS = 8 * 60 * 60 * 1_000;

function formatBeijingTime(epochMs) {
  if (!Number.isSafeInteger(epochMs) || epochMs < 0) return '时间不可用';
  const shiftedMs = epochMs + SHANGHAI_OFFSET_MS;
  if (!Number.isSafeInteger(shiftedMs)) return '时间不可用';
  const date = new Date(shiftedMs);
  if (Number.isNaN(date.getTime())) return '时间不可用';
  const pad = (value) => String(value).padStart(2, '0');
  return `${date.getUTCFullYear()}-${pad(date.getUTCMonth() + 1)}-${pad(date.getUTCDate())} `
    + `${pad(date.getUTCHours())}:${pad(date.getUTCMinutes())}:${pad(date.getUTCSeconds())}`;
}

function temperatureText(value) {
  return typeof value === 'number' && Number.isFinite(value)
    ? `${value.toFixed(1)}°C`
    : '无有效读数';
}

function eventName(event) {
  switch (event.type) {
    case 'high_temperature':
    case 'high_temperature_critical':
      return event.type === 'high_temperature_critical' || event.severity === 'n3'
        ? '严重高温'
        : '高温';
    case 'low_temperature':
    case 'low_temperature_critical':
      return event.type === 'low_temperature_critical' || event.severity === 'n3'
        ? '严重低温'
        : '低温';
    case 'sensor_fault':
      return '温度探头故障';
    case 'temperature_rapid_change':
      return event.severity === 'n3' ? '严重温度变化' : '温度变化过快';
    case 'temperature_gradient':
      return '主缸与回水缸温差';
    default:
      return '温度';
  }
}

function eventTitle(event) {
  if (event.type === 'sensor_fault' && event.state === 'opened') {
    return '温度探头异常告警';
  }
  if (event.type === 'sensor_fault' && event.state === 'resolved') {
    return '温度探头已恢复';
  }
  const name = eventName(event);
  switch (event.state) {
    case 'escalated':
      return `升级为严重${name.replace(/^严重/, '')}告警`;
    case 'reminder':
      return `${name}持续提醒`;
    case 'resolved':
      return `${name}告警已解除`;
    case 'opened':
    default:
      return `${name}告警`;
  }
}

function eventTimeLabel(state) {
  switch (state) {
    case 'escalated': return '升级判定';
    case 'reminder': return '提醒判定';
    case 'resolved': return '解除判定';
    case 'opened':
    default: return '告警判定';
  }
}

function messageFor(alert, sentAtMs = Date.now()) {
  if (alert.type === 'temperature') {
    const event = alert.event;
    const templates = {
      high_temperature: {
        action: '核查最新水温及加热棒温控。',
      },
      high_temperature_critical: {
        action: '核查最新水温及加热棒温控。',
      },
      low_temperature: {
        action: '核查最新水温及加热设备。',
      },
      low_temperature_critical: {
        action: '核查最新水温及加热设备。',
      },
      temperature_rapid_change: {
        action: '核查最新水温及探头位置。',
      },
      temperature_gradient: {
        action: '核查水流循环及探头位置。',
      },
      sensor_fault: {
        action: event.state === 'resolved' ? '继续观察水温。' : '检查探头、接线和防水接头。',
      },
    };
    const template = templates[event.type] ?? {
      action: '检查鱼缸监控设备。',
    };
    const readingLabel = event.type === 'temperature_gradient' ? '当时主缸水温' : '当时水温';
    const reading = event.type === 'sensor_fault' && event.state !== 'resolved'
      ? '无有效读数'
      : temperatureText(event.displayC);
    const stateLine = `${eventTimeLabel(event.state)}：事件时间不可用`;
    const sourceLine = `发送发起（云端）：${formatBeijingTime(sentAtMs)}`;
    let body = `${readingLabel}：${reading}\n${stateLine}\n${sourceLine}\n时间均为北京时间`;
    if (event.state === 'reminder') {
      body += '\n截至该次判定，尚未满足解除条件。';
    }
    if (event.type === 'sensor_fault' && event.state === 'resolved') {
      body += '\n探头已恢复。';
    }
    return {
      title: `${TEMPERATURE_DISPLAY_NAME}·主缸｜${eventTitle(event)}`,
      body: `${body}\n建议：${template.action}`,
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
  clock = Date.now,
}) {
  if (typeof barkKey !== 'string' || barkKey.length < 4) {
    throw new TypeError('A Bark key is required');
  }

  return {
    async send(alert) {
      const message = messageFor(alert, clock());
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
