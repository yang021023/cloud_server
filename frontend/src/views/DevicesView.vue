<template>
  <AppLayout>
    <section class="glass-panel page-panel">
      <div class="header-row">
        <div>
          <h3 class="section-title">{{ userStore.isAdmin ? '设备管理' : '我的专属设备' }}</h3>
          <p class="muted">{{ userStore.isAdmin ? '管理员可查看所有设备，也可配合用户管理按用户分配设备。' : '普通用户只能查看系统分配给自己的设备。' }}</p>
        </div>
        <el-input v-if="userStore.isAdmin" v-model="keyword" placeholder="按设备ID搜索" style="width: 240px" clearable />
      </div>

      <el-table :data="filteredDevices" stripe>
        <el-table-column prop="device_id" label="设备ID" width="160" />
        <el-table-column prop="device_name" label="设备名称" width="220" />
        <el-table-column prop="location" label="位置" />
        <el-table-column label="状态" width="100">
          <template #default="scope">
            <span :class="scope.row.online ? 'online' : 'offline'">{{ scope.row.online ? '在线' : '离线' }}</span>
          </template>
        </el-table-column>
        <el-table-column label="最后上报时间" width="220">
          <template #default="scope">
            {{ new Date(scope.row.last_seen * 1000).toLocaleString() }}
          </template>
        </el-table-column>
      </el-table>
    </section>
  </AppLayout>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import AppLayout from '@/components/AppLayout.vue'
import { getDevices } from '@/api/services'
import { useUserStore } from '@/stores/user'
import type { DeviceItem } from '@/types'

const userStore = useUserStore()
const devices = ref<DeviceItem[]>([])
const keyword = ref('')

const filteredDevices = computed(() => {
  const key = keyword.value.trim().toLowerCase()
  if (!key) return devices.value
  return devices.value.filter((item) => item.device_id.toLowerCase().includes(key))
})

async function loadDevices() {
  devices.value = await getDevices(userStore.username)
}

onMounted(loadDevices)
</script>

<style scoped>
.page-panel {
  padding: 20px;
}
.header-row {
  display: flex;
  justify-content: space-between;
  gap: 16px;
  align-items: center;
  margin-bottom: 20px;
}
.muted {
  color: var(--muted);
}
.online {
  color: #86efac;
}
.offline {
  color: #fda4af;
}
</style>
