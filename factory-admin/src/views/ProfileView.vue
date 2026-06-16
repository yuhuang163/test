<template>
  <el-row :gutter="24">
    <el-col :span="12">
      <el-card header="账号信息">
        <p>用户名：<strong>{{ user.username }}</strong></p>
        <p>角色：{{ roleText }}</p>
        <p>授权工站：{{ stationText }}</p>
        <el-button type="danger" plain @click="onLogout">退出登录</el-button>
      </el-card>
    </el-col>
    <el-col :span="12">
      <el-card header="修改密码">
        <el-form :model="form" label-width="90px" @submit.prevent="onChangePwd">
          <el-form-item label="旧密码">
            <el-input v-model="form.oldPassword" type="password" show-password />
          </el-form-item>
          <el-form-item label="新密码">
            <el-input v-model="form.newPassword" type="password" show-password />
          </el-form-item>
          <el-form-item label="确认新密码">
            <el-input v-model="form.confirmPassword" type="password" show-password />
          </el-form-item>
          <el-form-item>
            <el-button type="primary" :loading="loading" @click="onChangePwd">保存新密码</el-button>
          </el-form-item>
        </el-form>
      </el-card>
    </el-col>
  </el-row>
</template>

<script setup>
import { computed, reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'
import { useUserStore } from '../stores/user'
import * as api from '../api/users'

const router = useRouter()
const user = useUserStore()
const loading = ref(false)
const form = reactive({ oldPassword: '', newPassword: '', confirmPassword: '' })

const roleText = computed(() => {
  const map = { admin: '管理员', engineer: '工艺工程师', operator: '产线操作员' }
  return (user.roles || []).map((r) => map[r] || r).join('、') || '-'
})

const stationText = computed(() => (user.stationKeys?.length ? user.stationKeys.join('、') : '未限制'))

function onLogout() {
  user.logout()
  router.push('/login')
}

async function onChangePwd() {
  if (!form.oldPassword || !form.newPassword) {
    ElMessage.warning('请填写密码')
    return
  }
  if (form.newPassword !== form.confirmPassword) {
    ElMessage.warning('两次新密码不一致')
    return
  }
  loading.value = true
  try {
    await api.changePassword({
      oldPassword: form.oldPassword,
      newPassword: form.newPassword,
    })
    ElMessage.success('密码已修改，请重新登录')
    user.logout()
    router.push('/login')
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    loading.value = false
  }
}
</script>
