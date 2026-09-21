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
  const name = eventName(event);
  switch (event.state) {
    case 'escalated':
      return name;
    case 'reminder':
      return `${name}未解除`;
    case 'resolved':
      return `${name}已恢复`;
    case 'opened':
    default:
      return name;
  }
}

function severityText(event) {
  if (event.state === 'resolved') return '信息';
  return event.severity === 'n3' ? '严重' : '注意';
}

function notificationText(event) {
  if (event.state === 'resolved') return '恢复';
  if (event.state === 'reminder') return '重复';
  if (event.state === 'escalated'
      || (event.state === 'opened' && (event.notificationNumber ?? 1) > 1)) {
    return '升级';
  }
  return '首次';
}

function notificationNumber(event) {
  const value = event.notificationNumber ?? event.notification_number;
  return Number.isInteger(value) && value > 0 ? value : 1;
}

function countText(event) {
  if (event.state === 'resolved') return '不计入告警次数';
  const text = `本问题第 ${notificationNumber(event)} 次告警`;
  if (event.state === 'reminder') {
    const repeat = event.repeatNumber ?? event.repeat_number ?? 1;
    return `${text}；重复 ${repeat}/2`;
  }
  return event.severity === 'n3' ? text : `${text}；本问题不重复提醒`;
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
    const readingLabel = event.type === 'temperature_gradient' ? '当时主缸水温' : '当时水温';
    const reading = event.type === 'sensor_fault' && event.state !== 'resolved'
      ? '无有效读数'
      : temperatureText(event.displayC);
    const action = templates[event.type]?.action ?? '检查鱼缸监控设备。';
    const body = `设备：腾讯云\n情况：${readingLabel}：${reading}\n建议：${action}`
      + `\n时间：判定 事件时间不可用；发送：${formatBeijingTime(sentAtMs)}`
      + `（北京时间）\n次数：${countText(event)}`;
    return {
      title: `【${severityText(event)}·${notificationText(event)}】${TEMPERATURE_DISPLAY_NAME}·主缸｜${eventTitle(event)}`,
      body,
      level: event.state === 'resolved' || event.severity !== 'n3'
        ? 'active'
        : 'timeSensitive',
    };
  }
  if (alert.type === 'offline') {
    const minutes = Math.max(
      0,
      Math.floor((alert.detectedAtMs - alert.lastSeenAtMs) / 60_000),
    );
    return {
      title: `【注意·首次】鱼缸监控｜${alert.deviceId === 'esp1' ? 'ESP1' : alert.deviceId} 离线`,
      body: `设备：${alert.deviceId === 'esp1' ? 'ESP1' : alert.deviceId}（由腾讯云检测）\n`
        + `情况：已连续 ${minutes} 分钟未收到心跳。\n`
        + '建议：检查 Wi-Fi、设备供电和鱼缸现场。\n'
        + `时间：最后心跳 ${formatBeijingTime(alert.lastSeenAtMs)}；检测 ${formatBeijingTime(alert.detectedAtMs)}（北京时间）\n`
        + '次数：本次离线首次告警',
      level: 'timeSensitive',
    };
  }

  return {
    title: `【信息·恢复】鱼缸监控｜${alert.deviceId === 'esp1' ? 'ESP1' : alert.deviceId} 已恢复`,
    body: `设备：${alert.deviceId === 'esp1' ? 'ESP1' : alert.deviceId}（由腾讯云检测）\n`
      + '情况：云端已重新收到设备心跳。\n'
      + '建议：继续观察设备连接。\n'
      + `时间：恢复检测 ${formatBeijingTime(alert.detectedAtMs)}（北京时间）\n`
      + '次数：不计入告警次数',
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
