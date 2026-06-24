<template>
  <div class="login-page">
    <div class="bg-glow"></div>

    <el-card class="login-card" shadow="always">
      <div class="title-box">
        <div class="logo">⚙️</div>
        <h2>路特产线管理平台</h2>
        <p>Production Management System</p>
      </div>

      <el-form @submit.prevent="onSubmit" class="form">
        <el-form-item>
          <el-input
            v-model="username"
            placeholder="请输入用户名"
            autocomplete="username"
            size="large"
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
    </el-card>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'
import { useUserStore } from '../stores/user'

const router = useRouter()
const user = useUserStore()

const username = ref('admin')
const password = ref('admin123')
const loading = ref(false)

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
/* 背景 */
.login-page {
  height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  background: linear-gradient(135deg, #0f172a, #1e293b);
  position: relative;
  overflow: hidden;
}

/* 光晕 */
.bg-glow {
  position: absolute;
  width: 600px;
  height: 600px;
  background: radial-gradient(circle, rgba(59,130,246,0.3), transparent 60%);
  top: -200px;
  left: -200px;
  filter: blur(40px);
}

/* 卡片 */
.login-card {
  width: 380px;
  border-radius: 16px;
  backdrop-filter: blur(10px);
  background: rgba(255, 255, 255, 0.95);
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
  padding: 10px;
}

/* 标题 */
.title-box {
  text-align: center;
  margin-bottom: 25px;
}

.logo {
  font-size: 40px;
  margin-bottom: 10px;
}

h2 {
  margin: 0;
  font-size: 20px;
  font-weight: 600;
  color: #1f2937;
}

p {
  margin: 5px 0 0;
  font-size: 12px;
  color: #9ca3af;
}

/* 表单间距 */
.form {
  margin-top: 10px;
}

/* 按钮 */
.login-btn {
  width: 100%;
  height: 42px;
  border-radius: 10px;
  font-size: 15px;
  background: linear-gradient(135deg, #3b82f6, #2563eb);
  border: none;
  transition: all 0.2s ease;
}

.login-btn:hover {
  transform: translateY(-2px);
  box-shadow: 0 10px 20px rgba(59, 130, 246, 0.3);
}
</style>