<template>
  <AppLayout>
    <section class="glass-panel page-panel">
      <div class="toolbar">
        <el-select v-model="deviceId" placeholder="请选择设备" style="width: 240px">
          <el-option v-for="item in devices" :key="item.device_id" :label="item.device_id" :value="item.device_id" />
        </el-select>
        <el-button type="primary" @click="loadHistory" :disabled="!deviceId">查询历史</el-button>
      </div>
      <LineChart :title="'历史曲线'" :labels="labels" :series="series" />
    </section>
  </AppLayout>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import AppLayout from '@/components/AppLayout.vue'
import LineChart from '@/components/LineChart.vue'
import { getDevices, getHistoryData } from '@/api/services'
import { useUserStore } from '@/stores/user'
import type { DeviceItem, SensorData } from '@/types'

const userStore = useUserStore()
const deviceId = ref('')
const devices = ref<DeviceItem[]>([])
const history = ref<SensorData[]>([])

const labels = computed(() => history.value.map((item) => new Date(item.timestamp * 1000).toLocaleTimeString()))
const series = computed(() => [
  { name: '温度', data: history.value.map((item) => item.temperature), color: '#22c55e' },
  { name: '湿度', data: history.value.map((item) => item.humidity), color: '#38bdf8' },
  { name: '光照', data: history.value.map((item) => item.light), color: '#f59e0b' }
])

async function loadHistory() {
  if (!deviceId.value) {
    history.value = []
    return
  }
  history.value = await getHistoryData(deviceId.value, 100)
}

async function initPage() {
  devices.value = await getDevices(userStore.username)
  deviceId.value = devices.value[0]?.device_id || ''
  await loadHistory()
}

onMounted(initPage)
</script>

<style scoped>
.page-panel {
  padding: 20px;
}
.toolbar {
  display: flex;
  gap: 12px;
  margin-bottom: 20px;
  flex-wrap: wrap;
}
</style>
