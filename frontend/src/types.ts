export interface SensorData {
  device_id: string
  temperature: number
  humidity: number
  light: number
  timestamp: number
}

export interface LoginResponse {
  token: string
  role: 'admin' | 'viewer'
}

export interface AnalysisItem {
  device_id: string
  timestamp: number
  is_abnormal: boolean
  reason: string
  suggestion: string
}

export interface DeviceItem {
  device_id: string
  device_name?: string
  location?: string
  online: boolean
  last_seen: number
}

export interface AlertItem {
  id: number
  device_id: string
  timestamp: number
  alert_type: string
  value?: number
  message: string
  resolved: number
}

export interface RuleLogItem {
  device_id: string
  timestamp: number
  rule_name: string
  trigger_value?: number
  action: string
}

export interface CropSelection {
  device_id: string
  crop_name: string
  source: string
  updated_at: number
}

export interface CropProfile {
  crop_name: string
  min_temperature: number
  max_temperature: number
  min_humidity: number
  max_humidity: number
  min_light: number
  max_light: number
}

export interface CropValidationResponse {
  success: boolean
  exists: boolean
  crop_name: string
  message: string
  min_temperature: number
  max_temperature: number
  min_humidity: number
  max_humidity: number
  min_light: number
  max_light: number
}

export interface ControlCommandItem {
  id: number
  device_id: string
  timestamp: number
  crop_name: string
  command: string
  reason: string
  status: string
}

export interface UserItem {
  username: string
  role: 'admin' | 'viewer'
  last_login: number
}
