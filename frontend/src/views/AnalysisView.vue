<template>
  <AppLayout>
    <section class="glass-panel page-panel">
      <div class="toolbar">
        <el-select v-model="deviceId" placeholder="请选择设备" style="width: 240px">
          <el-option v-for="item in devices" :key="item.device_id" :label="item.device_id" :value="item.device_id" />
        </el-select>
        <el-select v-model="statusFilter" placeholder="状态筛选" style="width: 180px">
          <el-option label="全部状态" value="" />
          <el-option label="待执行" value="pending" />
          <el-option label="执行成功" value="success" />
          <el-option label="执行失败" value="failed" />
        </el-select>
        <el-button type="primary" @click="loadAnalysis" :disabled="!deviceId">查询分析</el-button>
      </div>

      <div class="analysis-grid">
        <div class="analysis-list">
          <div v-for="item in analysisList" :key="`${item.device_id}-${item.timestamp}`" class="analysis-card" :class="{ abnormal: item.is_abnormal }">
            <div class="head">
              <strong>{{ item.device_id }}</strong>
              <span>{{ new Date(item.timestamp * 1000).toLocaleString() }}</span>
            </div>
            <p><label>异常判定：</label>{{ item.is_abnormal ? '异常' : '正常' }}</p>
            <p><label>原因：</label>{{ translateReason(item.reason) }}</p>
            <p><label>建议：</label>{{ translateSuggestion(item.suggestion) }}</p>
          </div>
        </div>

        <div class="command-panel glass-panel">
          <div class="panel-head">
            <h3>云端控制指令</h3>
            <p>按当前作物 + AI 分析结果自动生成，并由边缘执行回执</p>
          </div>
          <div class="command-list">
            <div v-for="cmd in commands" :key="`${cmd.id}-${cmd.device_id}-${cmd.timestamp}`" class="command-card">
              <div class="head">
                <strong>#{{ cmd.id }} · {{ cmd.device_id }}</strong>
                <span>{{ new Date(cmd.timestamp * 1000).toLocaleString() }}</span>
              </div>
              <p><label>作物：</label>{{ cmd.crop_name }}</p>
              <p><label>指令：</label>{{ translateCommand(cmd.command) }}</p>
              <p><label>原因：</label>{{ translateReason(cmd.reason) }}</p>
              <p><label>状态：</label><span :class="statusClass(cmd.status)">{{ translateStatus(cmd.status) }}</span></p>
            </div>
          </div>
        </div>
      </div>
    </section>
  </AppLayout>
</template>

<script setup lang="ts">
import { onMounted, ref } from 'vue'
import AppLayout from '@/components/AppLayout.vue'
import { getAnalysis, getControlCommands, getDevices } from '@/api/services'
import { useUserStore } from '@/stores/user'
import type { AnalysisItem, ControlCommandItem, DeviceItem } from '@/types'
import { translateCommand, translateReason, translateStatus, translateSuggestion } from '@/utils/translate'

const userStore = useUserStore()
const deviceId = ref('')
const devices = ref<DeviceItem[]>([])
const statusFilter = ref('')
const analysisList = ref<AnalysisItem[]>([])
const commands = ref<ControlCommandItem[]>([])

async function loadAnalysis() {
  if (!deviceId.value) {
    analysisList.value = []
    commands.value = []
    return
  }
  analysisList.value = await getAnalysis(deviceId.value)
  commands.value = await getControlCommands(deviceId.value, statusFilter.value || undefined)
}

function statusClass(status: string) {
  if (status === 'success') return 'ok'
  if (status === 'failed') return 'danger'
  return 'pending'
}

async function initPage() {
  devices.value = await getDevices(userStore.username)
  deviceId.value = devices.value[0]?.device_id || ''
  await loadAnalysis()
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
.analysis-grid {
  display: grid;
  grid-template-columns: 1.15fr 0.85fr;
  gap: 18px;
  align-items: start;
}
.analysis-list,
.command-list {
  display: grid;
  gap: 14px;
}
.analysis-card,
.command-card {
  padding: 18px;
  border-radius: 18px;
  background: rgba(15, 23, 42, 0.6);
  border: 1px solid rgba(148, 163, 184, 0.12);
}
.analysis-card.abnormal {
  border-color: rgba(244, 63, 94, 0.5);
  box-shadow: inset 0 0 0 1px rgba(244, 63, 94, 0.25);
}
.command-panel {
  padding: 18px;
}
.panel-head {
  margin-bottom: 14px;
}
.panel-head h3 {
  margin: 0 0 6px;
}
.panel-head p,
label {
  color: var(--muted);
}
.head {
  display: flex;
  justify-content: space-between;
  margin-bottom: 10px;
  gap: 12px;
}
.ok {
  color: #86efac;
}
.pending {
  color: #facc15;
}
.danger {
  color: #fda4af;
}
@media (max-width: 1000px) {
  .analysis-grid {
    grid-template-columns: 1fr;
  }
}
</style>
