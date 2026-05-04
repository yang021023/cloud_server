<template>
  <AppLayout>
    <section class="glass-panel page-panel">
      <div class="toolbar">
        <el-select v-model="deviceId" placeholder="按设备筛选（可留空查看全部可见设备）" clearable style="width: 280px">
          <el-option v-for="item in devices" :key="item.device_id" :label="item.device_id" :value="item.device_id" />
        </el-select>
        <el-button type="primary" @click="loadAlerts">查询报警</el-button>
      </div>

      <el-table :data="alerts" stripe>
        <el-table-column prop="id" label="ID" width="80" />
        <el-table-column prop="device_id" label="设备ID" width="140" />
        <el-table-column label="报警类型" width="180">
          <template #default="scope">
            {{ translateAlertType(scope.row.alert_type) }}
          </template>
        </el-table-column>
        <el-table-column prop="value" label="触发值" width="120" />
        <el-table-column label="报警信息">
          <template #default="scope">
            {{ translateMessage(scope.row.message) }}
          </template>
        </el-table-column>
        <el-table-column label="状态" width="100">
          <template #default="scope">
            <span :class="scope.row.resolved ? 'ok' : 'danger'">{{ scope.row.resolved ? '已处理' : '未处理' }}</span>
          </template>
        </el-table-column>
        <el-table-column label="操作" width="120">
          <template #default="scope">
            <el-button v-if="!scope.row.resolved" size="small" type="danger" @click="markResolved(scope.row.id)">处理</el-button>
          </template>
        </el-table-column>
      </el-table>
    </section>
  </AppLayout>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { ElMessage } from 'element-plus'
import AppLayout from '@/components/AppLayout.vue'
import { getAlerts, getDevices, resolveAlert } from '@/api/services'
import { useUserStore } from '@/stores/user'
import type { AlertItem, DeviceItem } from '@/types'
import { translateAlertType, translateMessage } from '@/utils/translate'

const userStore = useUserStore()
const deviceId = ref('')
const devices = ref<DeviceItem[]>([])
const alerts = ref<AlertItem[]>([])

async function loadAlerts() {
  const allowedIds = new Set(devices.value.map((item) => item.device_id))
  const data = await getAlerts(deviceId.value || undefined)
  alerts.value = data.filter((item) => allowedIds.has(item.device_id))
}

async function markResolved(id: number) {
  await resolveAlert(id)
  ElMessage.success('报警已处理')
  await loadAlerts()
}

async function initPage() {
  devices.value = await getDevices(userStore.username)
  await loadAlerts()
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
.ok {
  color: #86efac;
}
.danger {
  color: #fda4af;
}
</style>
