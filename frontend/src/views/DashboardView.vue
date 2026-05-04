<template>
  <AppLayout>
    <div class="dashboard-grid">
      <section class="glass-panel crop-panel">
        <div class="crop-panel__text">
          <p class="crop-eyebrow">作物联动控制</p>
          <h3>设备 {{ selectedDeviceId || '未选择' }} 的当前作物：{{ cropState.crop_name || 'default' }}</h3>
          <p>
            来源：{{ cropState.source || 'default' }}
            <span v-if="cropState.updated_at"> · 更新时间：{{ formatTime(cropState.updated_at) }}</span>
          </p>
          <p v-if="currentCropProfile" class="crop-range">
            目标范围：温度 {{ formatRange(currentCropProfile.min_temperature, currentCropProfile.max_temperature, '℃') }}
            · 湿度 {{ formatRange(currentCropProfile.min_humidity, currentCropProfile.max_humidity, '%') }}
            · 光照 {{ formatRange(currentCropProfile.min_light, currentCropProfile.max_light, 'lux') }}
          </p>
          <p class="crop-tip">操作顺序：先选择装置，再选择已有作物；如果没有想要的作物，可手动输入，云端会先校验该作物是否存在，并返回对应温度、湿度、光照范围给边缘服务器。</p>
        </div>
        <div class="crop-panel__actions">
          <el-select v-model="selectedDeviceId" placeholder="第一步：选择装置" size="large" style="width: 210px" @change="handleDeviceChange">
            <el-option v-for="item in devices" :key="item.device_id" :label="item.device_id" :value="item.device_id" />
          </el-select>
          <div class="crop-input-group">
            <el-select
              v-model="selectedCrop"
              placeholder="第二步：选择已有作物"
              size="large"
              style="width: 220px"
              clearable
              :disabled="isCustomCropActive"
            >
              <el-option v-for="crop in presetCropOptions" :key="crop" :label="crop" :value="crop" />
            </el-select>
            <span v-if="isCustomCropActive" class="crop-inline-hint">当前以下发自定义作物为准</span>
          </div>
          <div class="crop-input-group">
            <el-input v-model="customCrop" placeholder="没有的话手动填写，如 grape" size="large" style="width: 260px" clearable />
            <span v-if="cropValidationMessage" :class="['crop-inline-hint', cropValidationOk ? 'is-success' : 'is-warning']">{{ cropValidationMessage }}</span>
          </div>
          <el-button type="success" size="large" :loading="cropSubmitting" @click="submitCrop">确认并下发</el-button>
        </div>
      </section>

      <section class="glass-panel card-grid">
        <div v-for="item in latestData" :key="item.device_id" class="device-card">
          <div class="device-head">
            <div>
              <h3>{{ item.device_id }}</h3>
              <p>最新时间：{{ formatTime(item.timestamp) }}</p>
              <p>当前作物：{{ cropMap[item.device_id]?.crop_name || 'default' }}</p>
              <p v-if="deviceCropProfiles[item.device_id]" class="device-range">
                目标：温度 {{ formatRange(deviceCropProfiles[item.device_id].min_temperature, deviceCropProfiles[item.device_id].max_temperature, '℃') }}
                · 湿度 {{ formatRange(deviceCropProfiles[item.device_id].min_humidity, deviceCropProfiles[item.device_id].max_humidity, '%') }}
                · 光照 {{ formatRange(deviceCropProfiles[item.device_id].min_light, deviceCropProfiles[item.device_id].max_light, 'lux') }}
              </p>
            </div>
            <span class="status" :class="deviceMap[item.device_id]?.online ? 'online' : 'offline'">
              {{ deviceMap[item.device_id]?.online ? '在线' : '离线' }}
            </span>
          </div>
          <div class="metrics">
            <div><label>温度</label><strong>{{ item.temperature.toFixed(2) }} ℃</strong></div>
            <div><label>湿度</label><strong>{{ item.humidity.toFixed(2) }} %</strong></div>
            <div><label>光照</label><strong>{{ item.light.toFixed(2) }} lux</strong></div>
          </div>
        </div>
      </section>

      <section class="glass-panel chart-panel">
        <h3 class="section-title">实时趋势（最近30条）</h3>
        <LineChart :title="'实时趋势'" :labels="chartLabels" :series="chartSeries" />
      </section>
    </div>
  </AppLayout>
</template>

<script setup lang="ts">
import { computed, onBeforeUnmount, onMounted, reactive, ref, watch } from 'vue'
import { ElMessage } from 'element-plus'
import AppLayout from '@/components/AppLayout.vue'
import LineChart from '@/components/LineChart.vue'
import { getAllCurrentCrops, getCropProfile, getCurrentCrop, getDevices, getLatestData, setCurrentCrop, validateCrop } from '@/api/services'
import { useUserStore } from '@/stores/user'
import type { CropProfile, CropSelection, DeviceItem, SensorData } from '@/types'

const userStore = useUserStore()
const latestData = ref<SensorData[]>([])
const devices = ref<DeviceItem[]>([])
const allCrops = ref<CropSelection[]>([])
const timer = ref<number | null>(null)
const cropSubmitting = ref(false)
const selectedDeviceId = ref('')
const selectedCrop = ref('default')
const customCrop = ref('')
const cropValidationMessage = ref('')
const cropValidationOk = ref(false)
const presetCropOptions = ['default', 'tomato', 'cucumber', 'pepper', 'strawberry', 'lettuce']
const cropProfiles = ref<Record<string, CropProfile>>({})
const cropState = reactive<CropSelection>({
  device_id: '',
  crop_name: '',
  source: 'default',
  updated_at: 0
})

const allowedDeviceIds = computed(() => new Set(devices.value.map((item) => item.device_id)))
const deviceMap = computed(() => Object.fromEntries(devices.value.map((item) => [item.device_id, item])))
const cropMap = computed(() => Object.fromEntries(allCrops.value.map((item) => [item.device_id, item])))
const isCustomCropActive = computed(() => customCrop.value.trim().length > 0)
const chartLabels = computed(() => latestData.value.map((item) => new Date(item.timestamp * 1000).toLocaleTimeString()))
const chartSeries = computed(() => [
  { name: '温度', data: latestData.value.map((item) => item.temperature), color: '#22c55e' },
  { name: '湿度', data: latestData.value.map((item) => item.humidity), color: '#38bdf8' },
  { name: '光照', data: latestData.value.map((item) => item.light), color: '#f59e0b' }
])
const currentCropProfile = computed(() => cropProfiles.value[cropState.crop_name || 'default'])
const deviceCropProfiles = computed(() => Object.fromEntries(
  latestData.value.map((item) => {
    const cropName = cropMap.value[item.device_id]?.crop_name || 'default'
    return [item.device_id, cropProfiles.value[cropName]]
  })
))

watch(customCrop, () => {
  cropValidationMessage.value = ''
  cropValidationOk.value = false
})

function formatTime(ts: number) {
  return new Date(ts * 1000).toLocaleString()
}

function formatRange(min: number, max: number, unit: string) {
  return `${min}~${max} ${unit}`
}

async function ensureCropProfile(cropName: string) {
  const key = cropName || 'default'
  if (cropProfiles.value[key]) return
  const profile = await getCropProfile(key)
  cropProfiles.value = {
    ...cropProfiles.value,
    [key]: profile
  }
}

async function syncVisibleCropProfiles() {
  const cropNames = new Set<string>()
  cropNames.add(cropState.crop_name || 'default')
  allCrops.value.forEach((item) => cropNames.add(item.crop_name || 'default'))
  await Promise.all(Array.from(cropNames).map((cropName) => ensureCropProfile(cropName)))
}

async function loadCropForDevice(options: { resetCustomCrop?: boolean } = {}) {
  if (!selectedDeviceId.value || !allowedDeviceIds.value.has(selectedDeviceId.value)) {
    return
  }
  Object.assign(cropState, await getCurrentCrop(selectedDeviceId.value))
  await ensureCropProfile(cropState.crop_name || 'default')
  if (!customCrop.value.trim()) {
    selectedCrop.value = cropState.crop_name || 'default'
  }
  if (options.resetCustomCrop) {
    customCrop.value = ''
    cropValidationMessage.value = ''
    cropValidationOk.value = false
  }
}

async function handleDeviceChange() {
  await loadCropForDevice({ resetCustomCrop: true })
}

async function loadData() {
  devices.value = await getDevices(userStore.username)

  const visibleIds = new Set(devices.value.map((item) => item.device_id))
  latestData.value = (await getLatestData()).filter((item) => visibleIds.has(item.device_id))
  allCrops.value = (await getAllCurrentCrops()).filter((item) => visibleIds.has(item.device_id))

  if (selectedDeviceId.value && !visibleIds.has(selectedDeviceId.value)) {
    selectedDeviceId.value = ''
    Object.assign(cropState, {
      device_id: '',
      crop_name: '',
      source: 'default',
      updated_at: 0
    })
  }

  if (!selectedDeviceId.value && devices.value.length) {
    selectedDeviceId.value = devices.value[0].device_id
    await loadCropForDevice({ resetCustomCrop: true })
    await syncVisibleCropProfiles()
    return
  }

  if (!devices.value.length) {
    latestData.value = []
    allCrops.value = []
    return
  }

  await loadCropForDevice()
  await syncVisibleCropProfiles()
}

async function submitCrop() {
  if (!selectedDeviceId.value) {
    ElMessage.warning('请先选择装置')
    return
  }

  const manualCrop = customCrop.value.trim()
  const cropName = manualCrop || selectedCrop.value.trim()
  if (!cropName) {
    ElMessage.warning('请选择已有作物或填写新的作物名称')
    return
  }

  cropSubmitting.value = true
  try {
    if (manualCrop) {
      const validation = await validateCrop(manualCrop)
      cropValidationMessage.value = validation.message
      cropValidationOk.value = validation.success && validation.exists
      if (!validation.success || !validation.exists) {
        ElMessage.warning(validation.message || '未识别该作物，请检查名称')
        return
      }

      const normalizedCrop = validation.crop_name || cropName
      await setCurrentCrop(selectedDeviceId.value, normalizedCrop, validation)
      cropProfiles.value = {
        ...cropProfiles.value,
        [normalizedCrop]: {
          crop_name: normalizedCrop,
          min_temperature: validation.min_temperature,
          max_temperature: validation.max_temperature,
          min_humidity: validation.min_humidity,
          max_humidity: validation.max_humidity,
          min_light: validation.min_light,
          max_light: validation.max_light
        }
      }
      selectedCrop.value = normalizedCrop
      customCrop.value = ''
      cropValidationMessage.value = ''
      cropValidationOk.value = false
      await loadData()
      ElMessage.success(`设备 ${selectedDeviceId.value} 作物已更新为 ${normalizedCrop}`)
      return
    }

    await setCurrentCrop(selectedDeviceId.value, cropName)
    await ensureCropProfile(cropName)
    selectedCrop.value = cropName
    customCrop.value = ''
    cropValidationMessage.value = ''
    cropValidationOk.value = false
    await loadData()
    ElMessage.success(`设备 ${selectedDeviceId.value} 作物已更新为 ${cropName}`)
  } catch (error) {
    ElMessage.error('作物下发失败，请检查云端服务')
  } finally {
    cropSubmitting.value = false
  }
}

onMounted(async () => {
  await loadData()
  timer.value = window.setInterval(loadData, 3000)
})

onBeforeUnmount(() => {
  if (timer.value) {
    window.clearInterval(timer.value)
  }
})
</script>

<style scoped>
.dashboard-grid {
  display: grid;
  gap: 20px;
}
.crop-panel {
  padding: 22px;
  display: flex;
  justify-content: space-between;
  gap: 18px;
  align-items: center;
  flex-wrap: wrap;
}
.crop-panel__text h3 {
  margin: 0 0 8px;
}
.crop-panel__text p {
  margin: 0;
  color: var(--muted);
}
.crop-range,
.device-range {
  margin-top: 10px !important;
  color: #cbd5e1 !important;
  font-size: 13px;
  line-height: 1.6;
}
.crop-tip {
  margin-top: 10px !important;
  max-width: 760px;
  line-height: 1.6;
}
.crop-eyebrow {
  margin: 0 0 10px;
  color: #86efac;
  text-transform: uppercase;
  letter-spacing: 0.18em;
  font-size: 12px;
}
.crop-panel__actions {
  display: flex;
  gap: 12px;
  flex-wrap: wrap;
  align-items: flex-start;
}
.crop-input-group {
  display: grid;
  gap: 6px;
}
.crop-inline-hint {
  font-size: 12px;
  color: #f59e0b;
  padding-left: 2px;
}
.crop-inline-hint.is-success {
  color: #86efac;
}
.crop-inline-hint.is-warning {
  color: #f59e0b;
}
.card-grid {
  padding: 20px;
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(250px, 1fr));
  gap: 16px;
}
.device-card {
  padding: 18px;
  border-radius: 20px;
  background: rgba(15, 23, 42, 0.68);
  border: 1px solid rgba(148, 163, 184, 0.16);
}
.device-head {
  display: flex;
  justify-content: space-between;
  gap: 10px;
}
.device-head h3 {
  margin: 0;
}
.device-head p {
  color: var(--muted);
  margin: 8px 0 0;
  font-size: 12px;
}
.status {
  padding: 6px 12px;
  height: fit-content;
  border-radius: 999px;
}
.online {
  background: rgba(34, 197, 94, 0.14);
  color: #86efac;
}
.offline {
  background: rgba(244, 63, 94, 0.14);
  color: #fda4af;
}
.metrics {
  margin-top: 16px;
  display: grid;
  gap: 12px;
}
.metrics label {
  display: block;
  color: var(--muted);
  margin-bottom: 4px;
}
.chart-panel {
  padding: 20px;
}
</style>
