<template>
  <div class="layout page-shell">
    <aside class="sidebar glass-panel">
      <div class="brand">
        <div class="brand-mark">GH</div>
        <div>
          <h1>温室云监控</h1>
          <p>Greenhouse Console</p>
        </div>
      </div>

      <nav class="nav-list">
        <RouterLink v-for="item in navItems" :key="item.path" :to="item.path" class="nav-item">
          <span>{{ item.label }}</span>
        </RouterLink>
      </nav>

      <div class="user-card glass-panel">
        <div>
          <strong>{{ userStore.username || '未登录' }}</strong>
          <p>{{ userStore.isAdmin ? 'admin · 管理员' : userStore.role || 'guest' }}</p>
        </div>
        <el-button type="danger" plain @click="logout">退出</el-button>
      </div>
    </aside>

    <main class="content">
      <header class="topbar glass-panel">
        <div>
          <h2>{{ title }}</h2>
          <p>{{ userStore.isAdmin ? '管理员可查看所有设备并管理用户设备归属' : '普通用户仅查看并调整自己的专属设备' }}</p>
        </div>
        <div class="status-pill">后端地址：127.0.0.1:8080</div>
      </header>

      <section class="view-panel">
        <slot />
      </section>
    </main>
  </div>
</template>

<script setup lang="ts">
import { computed } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useUserStore } from '@/stores/user'

const route = useRoute()
const router = useRouter()
const userStore = useUserStore()

const navItems = computed(() => {
  const items = [
    { path: '/dashboard', label: '实时监控' },
    { path: '/history', label: '历史数据' },
    { path: '/analysis', label: 'AI分析' },
    { path: '/alerts', label: '报警记录' },
    { path: '/devices', label: userStore.isAdmin ? '设备总览' : '我的设备' }
  ]
  if (userStore.isAdmin) {
    items.push({ path: '/users', label: '用户管理' })
  }
  return items
})

const titleMap: Record<string, string> = {
  '/dashboard': '实时监控面板',
  '/history': '历史数据回放',
  '/analysis': 'AI分析中心',
  '/alerts': '报警记录中心',
  '/devices': '设备管理',
  '/users': '用户管理'
}

const title = computed(() => titleMap[route.path] || '温室云监控')

function logout() {
  userStore.logout()
  router.push('/login')
}
</script>

<style scoped>
.layout {
  display: grid;
  grid-template-columns: 280px 1fr;
  gap: 20px;
  padding: 20px;
}
.sidebar {
  padding: 20px;
  display: flex;
  flex-direction: column;
  gap: 20px;
}
.brand {
  display: flex;
  align-items: center;
  gap: 14px;
}
.brand-mark {
  width: 56px;
  height: 56px;
  border-radius: 18px;
  display: grid;
  place-items: center;
  background: linear-gradient(135deg, #22c55e, #38bdf8);
  color: #03111f;
  font-weight: 800;
}
.brand h1 {
  margin: 0;
  font-size: 20px;
}
.brand p {
  margin: 4px 0 0;
  color: var(--muted);
  font-size: 12px;
}
.nav-list {
  display: flex;
  flex-direction: column;
  gap: 10px;
}
.nav-item {
  padding: 14px 16px;
  border-radius: 14px;
  color: #dbeafe;
  background: rgba(15, 23, 42, 0.55);
  border: 1px solid transparent;
  transition: all 0.2s ease;
}
.nav-item.router-link-active {
  border-color: rgba(56, 189, 248, 0.45);
  background: rgba(56, 189, 248, 0.16);
}
.user-card {
  margin-top: auto;
  padding: 16px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.user-card p {
  margin: 6px 0 0;
  color: var(--muted);
}
.content {
  display: flex;
  flex-direction: column;
  gap: 20px;
}
.topbar {
  padding: 22px 26px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}
.topbar h2 {
  margin: 0;
}
.topbar p {
  margin: 8px 0 0;
  color: var(--muted);
}
.status-pill {
  padding: 10px 14px;
  border-radius: 999px;
  background: rgba(34, 197, 94, 0.12);
  color: #86efac;
  border: 1px solid rgba(34, 197, 94, 0.22);
}
.view-panel {
  min-height: calc(100vh - 120px);
}
@media (max-width: 1080px) {
  .layout {
    grid-template-columns: 1fr;
  }
}
</style>
