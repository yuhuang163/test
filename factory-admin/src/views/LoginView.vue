<template>
  <div class="login-page">
    <div class="login-panel">
      <div class="brand">
        <div class="brand-icon">
          <svg viewBox="0 0 48 48" width="40" height="40" aria-hidden="true">
            <rect x="6" y="20" width="8" height="20" rx="2" fill="currentColor" opacity="0.45" />
            <rect x="20" y="10" width="8" height="30" rx="2" fill="currentColor" opacity="0.7" />
            <rect x="34" y="14" width="8" height="26" rx="2" fill="currentColor" />
            <line x1="4" y1="42" x2="44" y2="42" stroke="currentColor" stroke-width="2" stroke-linecap="round" />
          </svg>
        </div>
        <h1>路特产线管理平台</h1>
        <p>请使用管理员或工程师账号登录</p>
      </div>

      <el-form class="login-form" @submit.prevent="onSubmit">
        <el-form-item>
          <el-input
            v-model="username"
            placeholder="用户名"
            autocomplete="username"
            size="large"
            :prefix-icon="User"
            @keyup.enter="onSubmit"
          />
        </el-form-item>
        <el-form-item>
          <el-input
            v-model="password"
            type="password"
            show-password
            placeholder="密码"
            autocomplete="current-password"
            size="large"
            :prefix-icon="Lock"
            @keyup.enter="onSubmit"
          />
        </el-form-item>
        <el-button
          type="primary"
          size="large"
          class="submit-btn"
          native-type="submit"
          :loading="loading"
        >
          登录
        </el-button>
      </el-form>
    </div>
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

const username = ref('')
const password = ref('')
const loading = ref(false)

async function onSubmit() {
  if (!username.value.trim() || !password.value) {
    ElMessage.warning('请输入用户名和密码')
    return
  }
  loading.value = true
  try {
    await user.login(username.value.trim(), password.value)
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
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 24px;
  background: var(--admin-bg);
}

.login-panel {
  width: 100%;
  max-width: 400px;
  background: var(--admin-surface);
  border: 1px solid var(--admin-border);
  border-radius: var(--admin-radius-lg);
  padding: 36px 32px 32px;
  box-shadow: var(--admin-shadow);
}

.brand {
  text-align: center;
  margin-bottom: 28px;
}

.brand-icon {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  width: 56px;
  height: 56px;
  margin-bottom: 16px;
  border-radius: 14px;
  background: var(--admin-primary-light);
  color: var(--admin-primary);
}

.brand h1 {
  margin: 0;
  font-size: 20px;
  font-weight: 600;
  color: var(--admin-text);
  letter-spacing: 0.5px;
}

.brand p {
  margin: 8px 0 0;
  font-size: 13px;
  color: var(--admin-text-tertiary);
}

.login-form :deep(.el-form-item) {
  margin-bottom: 18px;
}

.submit-btn {
  width: 100%;
  margin-top: 4px;
  font-weight: 500;
}
</style>
