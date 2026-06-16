<template>
  <div>
    <el-row :gutter="16">
      <el-col :span="6" v-for="card in cards" :key="card.title">
        <el-card shadow="hover" class="stat-card" @click="go(card.path)">
          <div class="stat-title">{{ card.title }}</div>
          <div class="stat-desc">{{ card.desc }}</div>
        </el-card>
      </el-col>
    </el-row>
    <el-card class="welcome" shadow="never">
      <h3>欢迎使用路特产线管理平台</h3>
      <p>当前账号：<strong>{{ user.username }}</strong>，角色：{{ roleText }}</p>
      <p class="hint">左侧菜单进入各功能模块；配置类功能需 engineer / admin 权限。</p>
    </el-card>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useRouter } from 'vue-router'
import { useUserStore } from '../stores/user'
import { useRole } from '../composables/useRole'

const router = useRouter()
const user = useUserStore()
const { isAdmin, isEngineer } = useRole()

const roleText = computed(() => {
  const r = user.roles || []
  if (!r.length) return '未分配'
  const map = { admin: '管理员', engineer: '工艺工程师', operator: '产线操作员' }
  return r.map((x) => map[x] || x).join('、')
})

const cards = computed(() => {
  const list = [
    { title: '日志查询', desc: '查看产线上传日志', path: '/data/logs' },
  ]
  if (isEngineer.value) {
    list.push(
      { title: '阈值模板', desc: '维护卡控上下限', path: '/config/thresholds' },
      { title: '测试用例', desc: '编辑 test_case ini', path: '/config/test-cases' },
      { title: '统一发布', desc: '组合阈值/用例/exe', path: '/config/releases' }
    )
  }
  if (isAdmin.value) {
    list.push(
      { title: '上位机版本', desc: 'OTA 发版管理', path: '/config/host-app' },
      { title: '账号管理', desc: '用户与工站授权', path: '/system/users' }
    )
  }
  return list
})

function go(path) {
  router.push(path)
}
</script>

<style scoped>
.stat-card { cursor: pointer; margin-bottom: 16px; }
.stat-title { font-size: 16px; font-weight: 600; margin-bottom: 8px; }
.stat-desc { color: #666; font-size: 13px; }
.welcome { margin-top: 8px; }
.welcome h3 { margin: 0 0 12px; }
.hint { color: #888; font-size: 13px; }
</style>
