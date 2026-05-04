export function translateCommand(value: string) {
  const text = value || ''
  const mapping: Record<string, string> = {
    TURN_ON_FAN: '开启风扇',
    START_IRRIGATION: '开始灌溉',
    TURN_ON_GROW_LIGHT: '开启补光灯',
    KEEP_STABLE: '保持当前环境',
    FAN_ON: '开启风扇',
    IRRIGATION_ON: '开始灌溉',
    GROW_LIGHT_ON: '开启补光灯',
    SHADE_ON: '启动遮阳',
    HEATER_ON: '开启加热',
    DEHUMIDIFIER_ON: '启动除湿',
    ENVIRONMENT_STABLE: '环境稳定'
  }
  return mapping[text] || text
}

export function translateStatus(value: string) {
  const mapping: Record<string, string> = {
    pending: '待执行',
    success: '执行成功',
    failed: '执行失败'
  }
  return mapping[value] || value
}

export function translateReason(value: string) {
  const text = value || ''
  if (!text) return '-'
  if (text.includes('HIGH_TEMPERATURE')) return '温度过高'
  if (text.includes('LOW_TEMPERATURE')) return '温度过低'
  if (text.includes('LOW_HUMIDITY')) return '湿度过低'
  if (text.includes('HIGH_HUMIDITY')) return '湿度过高'
  if (text.includes('LOW_LIGHT')) return '光照不足'
  if (text.includes('HIGH_LIGHT')) return '光照过强'
  if (text.includes('NORMAL')) return '环境正常'
  return text.replaceAll('_', ' ')
}

export function translateSuggestion(value: string) {
  const text = value || ''
  if (!text) return '-'
  if (text.includes('TURN_ON_FAN')) return '建议开启风扇降温'
  if (text.includes('START_IRRIGATION')) return '建议开始灌溉，提高土壤或空气湿度'
  if (text.includes('TURN_ON_GROW_LIGHT')) return '建议开启补光灯'
  if (text.includes('KEEP_CURRENT_ENVIRONMENT') || text.includes('KEEP_STABLE')) return '建议保持当前环境'
  return translateCommand(text)
}

export function translateAlertType(value: string) {
  const mapping: Record<string, string> = {
    temperature_high: '高温报警',
    temperature_low: '低温报警',
    humidity_high: '高湿报警',
    humidity_low: '低湿报警',
    light_high: '强光报警',
    light_low: '弱光报警'
  }
  return mapping[value] || value
}

export function translateMessage(value: string) {
  const text = value || ''
  if (!text) return '-'
  if (text.includes('HIGH_TEMPERATURE')) return '检测到温度过高'
  if (text.includes('LOW_TEMPERATURE')) return '检测到温度过低'
  if (text.includes('LOW_HUMIDITY')) return '检测到湿度过低'
  if (text.includes('HIGH_HUMIDITY')) return '检测到湿度过高'
  if (text.includes('LOW_LIGHT')) return '检测到光照不足'
  if (text.includes('HIGH_LIGHT')) return '检测到光照过强'
  return text.replaceAll('_', ' ')
}
