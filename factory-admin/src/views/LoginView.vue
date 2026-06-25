<template>
  <div class="login-page">
    <div class="bg-bubbles">
      <span v-for="n in 12" :key="n" class="bubble" :style="bubbleStyle(n)" />
    </div>

    <el-card class="login-card" shadow="never">
      <div class="title-box">
        <div class="logo">
          <svg viewBox="0 0 48 48" width="48" height="48">
            <rect x="6" y="20" width="8" height="20" rx="2" fill="#3b82f6" opacity="0.6"/>
            <rect x="20" y="10" width="8" height="30" rx="2" fill="#3b82f6" opacity="0.8"/>
            <rect x="34" y="14" width="8" height="26" rx="2" fill="#3b82f6"/>
            <line x1="4" y1="42" x2="44" y2="42" stroke="#3b82f6" stroke-width="2" stroke-linecap="round"/>
          </svg>
        </div>
        <h2>路特产线管理平台</h2>
        <p class="subtitle">Production Management System</p>
      </div>

      <el-form @submit.prevent="onSubmit" class="form">
        <el-form-item>
          <el-input
            v-model="username"
            placeholder="请输入用户名"
            autocomplete="username"
            size="large"
            :prefix-icon="User"
          />
        </el-form-item>

        <el-form-item>
          <el-input
            v-model="password"
            type="password"
            show-password
            placeholder="请输入密码"
            autocomplete="current-password"
            size="large"
            :prefix-icon="Lock"
          />
        </el-form-item>

        <el-button
          type="primary"
          :loading="loading"
          class="login-btn"
          @click="onSubmit"
        >
          登录系统
        </el-button>
      </el-form>

      <div class="footer">
        <span class="version">v1.0.0</span>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'
import { User, Lock } from '@element-plus/icons-vue'
import { useUserStore } from '../stores/user'

const router = useRouter()
const user = useUserStore()

const username = ref('admin')
const password = ref('admin123')
const loading = ref(false)

function bubbleStyle(n) {
  const size = 20 + Math.random() * 60
  const left = 5 + (n - 1) * 8
  const delay = Math.random() * 12
  const duration = 8 + Math.random() * 8
  return {
    width: `${size}px`,
    height: `${size}px`,
    left: `${left}%`,
    animationDelay: `${delay}s`,
    animationDuration: `${duration}s`,
  }
}

async function onSubmit() {
  loading.value = true
  try {
    await user.login(username.value, password.value)
    ElMessage.success('登录成功')
    router.push('/dashboard')
  } catch (e) {
    ElMessage.error(e.message || '登录失败')
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.login-page {
  height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  background: linear-gradient(135deg, #0b1120 0%, #15203f 50%, #0f172a 100%);
  position: relative;
  overflow: hidden;
}

/* 气泡动画 */
.bg-bubbles {
  position: absolute;
  inset: 0;
  pointer-events: none;
}
.bubble {
  position: absolute;
  bottom: -80px;
  background: radial-gradient(circle at 30% 30%, rgba(59,130,246,0.15), transparent);
  border-radius: 50%;
  animation: rise linear infinite;
}
@keyframes rise {
  0% { transform: translateY(0) scale(1); opacity: 0; }
  10% { opacity: 0.4; }
  90% { opacity: 0.2; }
  100% { transform: translateY(-110vh) scale(0.6); opacity: 0; }
}

.login-card {
  width: 400px;
  border-radius: 20px;
  background: rgba(255,255,255,0.96);
  backdrop-filter: blur(20px);
  box-shadow: 0 25px 80px rgba(0,0,0,0.5);
  padding: 12px;
  position: relative;
  z-index: 1;
}

.title-box {
  text-align: center;
  margin-bottom: 28px;
}
.logo {
  margin-bottom: 14px;
  display: flex;
  justify-content: center;
}
h2 {
  margin: 0;
  font-size: 22px;
  font-weight: 700;
  color: #1e293b;
  letter-spacing: 1px;
}
.subtitle {
  margin: 6px 0 0;
  font-size: 13px;
  color: #94a3b8;
  letter-spacing: 2px;
}

.form {
  margin-top: 8px;
}

:deep(.el-input__wrapper) {
  border-radius: 12px;
  padding: 4px 16px;
}
:deep(.el-input__prefix) {
  margin-right: 8px;
}

.login-btn {
  width: 100%;
  height: 46px;
  border-radius: 12px;
  font-size: 16px;
  font-weight: 600;
  background: linear-gradient(135deg, #3b82f6, #1d4ed8);
  border: none;
  letter-spacing: 2px;
  transition: all 0.25s ease;
  margin-top: 8px;
}
.login-btn:hover {
  transform: translateY(-2px);
  box-shadow: 0 12px 28px rgba(59,130,246,0.35);
}
.login-btn:active {
  transform: translateY(0);
}

.footer {
  text-align: center;
  margin-top: 22px;
}
.version {
  font-size: 12px;
  color: #cbd5e1;
}
</style>
