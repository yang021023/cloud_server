import { createRouter, createWebHistory } from 'vue-router'
import { useUserStore } from '@/stores/user'

const routes = [
  { path: '/', redirect: '/dashboard' },
  { path: '/login', component: () => import('@/views/LoginView.vue') },
  { path: '/dashboard', component: () => import('@/views/DashboardView.vue') },
  { path: '/history', component: () => import('@/views/HistoryView.vue') },
  { path: '/analysis', component: () => import('@/views/AnalysisView.vue') },
  { path: '/alerts', component: () => import('@/views/AlertsView.vue') },
  { path: '/devices', component: () => import('@/views/DevicesView.vue') },
  { path: '/users', component: () => import('@/views/UsersView.vue'), meta: { adminOnly: true } }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

router.beforeEach((to) => {
  const userStore = useUserStore()
  if (to.path !== '/login' && !userStore.isLoggedIn) {
    return '/login'
  }
  if (to.path === '/login' && userStore.isLoggedIn) {
    return '/dashboard'
  }
  if (to.meta.adminOnly && !userStore.isAdmin) {
    return '/dashboard'
  }
  return true
})

export default router
