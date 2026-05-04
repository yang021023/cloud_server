import http from './http'
import type { AlertItem, AnalysisItem, ControlCommandItem, CropProfile, CropSelection, CropValidationResponse, DeviceItem, LoginResponse, RuleLogItem, SensorData, UserItem } from '@/types'

export async function login(username: string, password: string) {
  const { data } = await http.post<LoginResponse>('/api/login', { username, password })
  return data
}

export async function register(username: string, password: string) {
  const { data } = await http.post('/api/register', { username, password })
  return data
}

export async function getUsers() {
  const { data } = await http.get<UserItem[]>('/api/users')
  return data
}

export async function getUserDevices(username: string) {
  const { data } = await http.get<string[]>('/api/user_devices', { params: { username } })
  return data
}

export async function setUserDevices(username: string, deviceIds: string[]) {
  const { data } = await http.post('/api/user_devices', {
    username,
    device_ids: deviceIds.join(',')
  })
  return data
}

export async function getLatestData() {
  const { data } = await http.get<SensorData[]>('/api/latest')
  return data
}

export async function getHistoryData(deviceId: string, limit = 100) {
  const { data } = await http.get<SensorData[]>('/api/history', {
    params: { device_id: deviceId, limit }
  })
  return data
}

export async function getAnalysis(deviceId: string) {
  const { data } = await http.get<AnalysisItem[]>('/api/analysis', {
    params: { device_id: deviceId }
  })
  return data
}

export async function getDevices(username?: string) {
  const { data } = await http.get<DeviceItem[]>('/api/devices', {
    params: username ? { username } : {}
  })
  return data
}

export async function getAlerts(deviceId?: string) {
  const { data } = await http.get<AlertItem[]>('/api/alerts', {
    params: deviceId ? { device_id: deviceId } : {}
  })
  return data
}

export async function resolveAlert(id: number) {
  const { data } = await http.post(`/api/alerts/${id}/resolve`)
  return data
}

export async function getRuleLogs(deviceId?: string) {
  const { data } = await http.get<RuleLogItem[]>('/api/rule_logs', {
    params: deviceId ? { device_id: deviceId } : {}
  })
  return data
}

export async function getControlCommands(deviceId?: string, status?: string) {
  const params: Record<string, string> = {}
  if (deviceId) params.device_id = deviceId
  if (status) params.status = status
  const { data } = await http.get<ControlCommandItem[]>('/api/control_commands', { params })
  return data
}

export async function getCurrentCrop(deviceId: string) {
  const { data } = await http.get<CropSelection>('/api/crop', {
    params: { device_id: deviceId }
  })
  return data
}

export async function getAllCurrentCrops() {
  const { data } = await http.get<CropSelection[]>('/api/crop')
  return data
}

export async function getCropProfile(cropName: string) {
  const { data } = await http.get<CropProfile>('/api/crop_profile', {
    params: { crop_name: cropName }
  })
  return data
}

export async function validateCrop(cropName: string) {
  const { data } = await http.post<CropValidationResponse>('/api/crop/validate', { crop_name: cropName })
  return data
}

export async function setCurrentCrop(deviceId: string, cropName: string, profile?: Partial<CropValidationResponse>) {
  const payload: Record<string, string> = {
    device_id: deviceId,
    crop_name: cropName
  }

  if (profile) {
    if (profile.min_temperature != null) payload.min_temperature = String(profile.min_temperature)
    if (profile.max_temperature != null) payload.max_temperature = String(profile.max_temperature)
    if (profile.min_humidity != null) payload.min_humidity = String(profile.min_humidity)
    if (profile.max_humidity != null) payload.max_humidity = String(profile.max_humidity)
    if (profile.min_light != null) payload.min_light = String(profile.min_light)
    if (profile.max_light != null) payload.max_light = String(profile.max_light)
  }

  const { data } = await http.post('/api/crop', payload)
  return data
}
