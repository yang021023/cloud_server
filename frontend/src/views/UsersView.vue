<template>
  <AppLayout>
    <section class="glass-panel page-panel">
      <div class="header-row">
        <div>
          <h3 class="section-title">用户管理</h3>
          <p class="muted">管理员可查看所有用户，并为普通用户分配专属设备。</p>
        </div>
        <el-input v-model="keyword" placeholder="按用户ID搜索" style="width: 260px" clearable />
      </div>

      <el-table :data="filteredUsers" stripe>
        <el-table-column prop="username" label="用户ID" width="180" />
        <el-table-column prop="role" label="角色" width="120" />
        <el-table-column label="最后登录时间" width="220">
          <template #default="scope">
            {{ scope.row.last_login ? new Date(scope.row.last_login * 1000).toLocaleString() : '未登录' }}
          </template>
        </el-table-column>
        <el-table-column label="专属设备">
          <template #default="scope">
            <div class="device-tags">
              <el-tag v-for="item in assignedMap[scope.row.username] || []" :key="item" effect="plain">{{ item }}</el-tag>
              <span v-if="!(assignedMap[scope.row.username] || []).length" class="muted">未分配</span>
            </div>
          </template>
        </el-table-column>
        <el-table-column label="管理" width="300">
          <template #default="scope">
            <el-select
              :model-value="assignedMap[scope.row.username] || []"
              multiple
              collapse-tags
              collapse-tags-tooltip
              placeholder="选择设备"
              style="width: 220px"
              @change="(value) => updateAssignedDevices(scope.row.username, value as string[])"
              :disabled="scope.row.role === 'admin'"
            >
              <el-option v-for="device in devices" :key="device.device_id" :label="device.device_id" :value="device.device_id" />
            </el-select>
          </template>
        </el-table-column>
      </el-table>
    </section>
  </AppLayout>
</template>

<script setup lang="ts">
import { computed, onMounted, ref } from 'vue'
import { ElMessage } from 'element-plus'
import AppLayout from '@/components/AppLayout.vue'
import { getDevices, getUserDevices, getUsers, setUserDevices } from '@/api/services'
import type { DeviceItem, UserItem } from '@/types'

const users = ref<UserItem[]>([])
const devices = ref<DeviceItem[]>([])
const keyword = ref('')
const assignedMap = ref<Record<string, string[]>>({})

const filteredUsers = computed(() => {
  const key = keyword.value.trim().toLowerCase()
  if (!key) return users.value
  return users.value.filter((item) => item.username.toLowerCase().includes(key))
})

async function loadData() {
  users.value = await getUsers()
  devices.value = await getDevices()
  const entries = await Promise.all(users.value.map(async (user) => [user.username, await getUserDevices(user.username)] as const))
  assignedMap.value = Object.fromEntries(entries)
}

async function updateAssignedDevices(username: string, deviceIds: string[]) {
  try {
    await setUserDevices(username, deviceIds)
    assignedMap.value = {
      ...assignedMap.value,
      [username]: deviceIds
    }
    ElMessage.success(`已更新 ${username} 的设备分配`)
  } catch (error) {
    ElMessage.error('设备分配失败')
  }
}

onMounted(loadData)
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
.device-tags {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}
</style>
