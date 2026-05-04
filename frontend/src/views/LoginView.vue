<template>
  <div class="login-page">
    <div class="login-backdrop"></div>
    <div class="login-card glass-panel">
      <div class="hero-block">
        <p class="eyebrow">GREENHOUSE EDGE CONSOLE</p>
        <h1>温室大棚边缘云协同平台</h1>
        <p class="desc">区分管理员与普通用户登录。普通用户可自助注册，管理员账号仅保留系统初始化账户。</p>
        <div class="hero-meta">
          <span>管理员可分配设备</span>
          <span>普通用户仅查看自己的设备</span>
          <span>边缘与云端联动控制</span>
        </div>
      </div>

      <div class="form-shell">
        <div class="tab-switch">
          <button :class="['tab-btn', mode === 'login' ? 'is-active' : '']" @click="mode = 'login'">登录</button>
          <button :class="['tab-btn', mode === 'register' ? 'is-active' : '']" @click="mode = 'register'">普通用户注册</button>
        </div>

        <el-form :model="form" @submit.prevent="mode === 'login' ? handleLogin() : handleRegister()">
          <el-form-item>
            <el-input v-model="form.username" placeholder="用户名" size="large" />
          </el-form-item>
          <el-form-item>
            <el-input v-model="form.password" type="password" placeholder="密码" size="large" show-password />
          </el-form-item>
          <el-button v-if="mode === 'login'" class="submit-btn" type="success" size="large" :loading="loading" @click="handleLogin">
            登录系统
          </el-button>
          <el-button v-else class="submit-btn" type="primary" size="large" :loading="loading" @click="handleRegister">
            注册普通用户
          </el-button>
        </el-form>

        <div class="tips">
          <template v-if="mode === 'login'">
            默认管理员账号：<strong>admin</strong> / <strong>admin123</strong>
          </template>
          <template v-else>
            注册入口仅创建普通用户，管理员账号不能注册。
          </template>
        </div>
      </div>
    </div>
  </div>
</template>

<script setup lang="ts">
import { reactive, ref } from 'vue'
import { ElMessage } from 'element-plus'
import { useRouter } from 'vue-router'
import { login, register } from '@/api/services'
import { useUserStore } from '@/stores/user'

const router = useRouter()
const userStore = useUserStore()
const loading = ref(false)
const mode = ref<'login' | 'register'>('login')
const form = reactive({
  username: 'admin',
  password: 'admin123'
})

async function handleLogin() {
  if (!form.username || !form.password) {
    ElMessage.warning('请输入用户名和密码')
    return
  }

  loading.value = true
  try {
    const result = await login(form.username, form.password)
    userStore.setAuth(result.token, result.role, form.username)
    ElMessage.success(result.role === 'admin' ? '管理员登录成功' : '用户登录成功')
    router.push('/dashboard')
  } catch (error) {
    ElMessage.error('登录失败，请检查用户名或密码')
  } finally {
    loading.value = false
  }
}

async function handleRegister() {
  if (!form.username || !form.password) {
    ElMessage.warning('请输入用户名和密码')
    return
  }

  loading.value = true
  try {
    await register(form.username, form.password)
    ElMessage.success('普通用户注册成功，请登录')
    mode.value = 'login'
  } catch (error) {
    ElMessage.error('注册失败，用户名可能已存在')
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.login-page {
  min-height: 100vh;
  display: grid;
  place-items: center;
  padding: 24px;
  position: relative;
  overflow: hidden;
  background: radial-gradient(circle at top, rgba(34, 197, 94, 0.18), transparent 28%), #07111f;
}
.login-backdrop {
  position: absolute;
  inset: 0;
  background-image:
    linear-gradient(rgba(148, 163, 184, 0.06) 1px, transparent 1px),
    linear-gradient(90deg, rgba(148, 163, 184, 0.06) 1px, transparent 1px);
  background-size: 28px 28px;
  mask-image: radial-gradient(circle at center, black, transparent 85%);
}
.login-card {
  position: relative;
  z-index: 1;
  width: min(1080px, 100%);
  display: grid;
  grid-template-columns: 1.15fr 0.9fr;
  gap: 28px;
  padding: 30px;
}
.hero-block {
  padding: 18px;
  display: flex;
  flex-direction: column;
  justify-content: center;
}
.eyebrow {
  margin: 0 0 14px;
  color: #86efac;
  font-size: 12px;
  letter-spacing: 0.24em;
}
.hero-block h1 {
  margin: 0;
  font-size: 44px;
  line-height: 1.15;
}
.desc {
  color: #cbd5e1;
  line-height: 1.8;
  margin-top: 18px;
  max-width: 560px;
}
.hero-meta {
  margin-top: 22px;
  display: flex;
  flex-wrap: wrap;
  gap: 10px;
}
.hero-meta span {
  padding: 10px 14px;
  border-radius: 999px;
  background: rgba(15, 23, 42, 0.64);
  border: 1px solid rgba(56, 189, 248, 0.2);
  color: #dbeafe;
}
.form-shell {
  padding: 24px;
  border-radius: 26px;
  background: rgba(6, 14, 27, 0.72);
  border: 1px solid rgba(148, 163, 184, 0.14);
}
.tab-switch {
  display: flex;
  margin-bottom: 20px;
  background: rgba(15, 23, 42, 0.72);
  border-radius: 16px;
  padding: 6px;
}
.tab-btn {
  flex: 1;
  border: 0;
  background: transparent;
  color: #cbd5e1;
  padding: 12px 14px;
  border-radius: 12px;
  cursor: pointer;
}
.tab-btn.is-active {
  background: linear-gradient(135deg, rgba(34, 197, 94, 0.24), rgba(56, 189, 248, 0.24));
  color: white;
}
.submit-btn {
  width: 100%;
}
.tips {
  margin-top: 18px;
  color: var(--muted);
  line-height: 1.7;
}
@media (max-width: 960px) {
  .login-card {
    grid-template-columns: 1fr;
  }
  .hero-block h1 {
    font-size: 34px;
  }
}
</style>
