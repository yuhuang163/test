<template>
  <div class="dashboard">
    <div class="welcome-card">
      <div class="welcome-text">
        <h3>欢迎回来，{{ user.username }}</h3>
        <p>角色：{{ roleText }} · 当前账号可在左侧菜单进入各功能模块</p>
      </div>
      <div class="welcome-icon">
        <svg viewBox="0 0 120 80" width="140" height="90">
          <rect x="10" y="30" width="20" height="40" rx="3" fill="#3b82f6" opacity="0.3"/>
          <rect x="38" y="15" width="20" height="55" rx="3" fill="#3b82f6" opacity="0.55"/>
          <rect x="66" y="22" width="20" height="48" rx="3" fill="#3b82f6" opacity="0.75"/>
          <rect x="94" y="8" width="20" height="62" rx="3" fill="#3b82f6"/>
          <line x1="6" y1="74" x2="114" y2="74" stroke="#3b82f6" stroke-width="2" stroke-linecap="round" opacity="0.5"/>
        </svg>
      </div>
    </div>

    <el-row :gutter="16">
      <el-col :xs="24" :sm="12" :md="6" v-for="card in cards" :key="card.title">
        <el-card shadow="never" class="stat-card" @click="go(card.path)">
          <div class="stat-icon" :style="{ background: card.bg }">
            <el-icon :size="24" color="#fff">
              <component :is="card.icon" />
            </el-icon>
          </div>
          <div class="stat-body">
            <div class="stat-title">{{ card.title }}</div>
            <div class="stat-desc">{{ card.desc }}</div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="info-row">
      <el-col :span="12">
        <el-card shadow="never" class="info-card">
          <template #header>
            <span class="info-card-title">系统信息</span>
          </template>
          <div class="info-item"><span class="label">平台版本</span><span>v1.0.0</span></div>
          <div class="info-item"><span class="label">当前用户</span><span>{{ user.username }}</span></div>
          <div class="info-item"><span class="label">用户角色</span><span>{{ roleText }}</span></div>
          <div class="info-item"><span class="label">服务状态</span><el-tag size="small" type="success">运行中</el-tag></div>
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card shadow="never" class="info-card">
          <template #header>
            <span class="info-card-title">快速入口</span>
          </template>
          <el-button v-for="link in quickLinks" :key="link.title" class="quick-btn" @click="go(link.path)">
            <el-icon><component :is="link.icon" /></el-icon>
            {{ link.title }}
          </el-button>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { useRouter } from 'vue-router'
import { useUserStore } from '../stores/user'
import { useRole } from '../composables/useRole'
import {
  Document, Edit, UserFilled, Upload,
  Files, Tools, Monitor, Clock
} from '@element-plus/icons-vue'

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
    { title: '日志查询', desc: '查看产线上传的操作日志', path: '/data/logs', icon: Document, bg: 'linear-gradient(135deg, #3b82f6, #2563eb)' },
  ]
  if (isEngineer.value) {
    list.push(
      { title: '测试用例', desc: '编辑与发布测试用例', path: '/config/test-cases', icon: Edit, bg: 'linear-gradient(135deg, #8b5cf6, #7c3aed)' },
    )
  }
  if (isAdmin.value) {
    list.push(
      { title: '上位机版本', desc: '发版与运行环境管理', path: '/config/host-app', icon: Upload, bg: 'linear-gradient(135deg, #10b981, #059669)' },
      { title: '账号管理', desc: '用户与工站授权管理', path: '/system/users', icon: UserFilled, bg: 'linear-gradient(135deg, #f59e0b, #d97706)' },
    )
  }
  return list
})

const quickLinks = computed(() => {
  const links = []
  if (isAdmin.value) {
    links.push({ title: '设备登记', path: '/system/devices', icon: Tools })
    links.push({ title: '登录审计', path: '/system/audit-logins', icon: Clock })
  }
  if (isEngineer.value) {
    links.push({ title: '测试用例', path: '/config/test-cases', icon: Files })
  }
  links.push({ title: '日志查询', path: '/data/logs', icon: Document })
  return links
})

function go(path) {
  router.push(path)
}
</script>

<style scoped>
.dashboard {
  max-width: 1200px;
}

.welcome-card {
  background: linear-gradient(135deg, #1e293b, #0f172a);
  border-radius: 16px;
  padding: 28px 32px;
  margin-bottom: 20px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  color: #fff;
}
.welcome-text h3 {
  margin: 0 0 8px;
  font-size: 20px;
  font-weight: 700;
}
.welcome-text p {
  margin: 0;
  color: rgba(255,255,255,0.6);
  font-size: 14px;
}
.welcome-icon {
  opacity: 0.5;
}

/* 统计卡片 */
.stat-card {
  cursor: pointer;
  margin-bottom: 16px;
  border-radius: 14px;
  border: none;
  display: flex;
  align-items: center;
  padding: 16px;
  transition: all 0.2s ease;
  background: #fff;
}
.stat-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 12px 28px rgba(0,0,0,0.08);
}

.stat-icon {
  width: 48px;
  height: 48px;
  border-radius: 14px;
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  margin-right: 14px;
}

.stat-body {
  flex: 1;
  min-width: 0;
}
.stat-title {
  font-size: 15px;
  font-weight: 600;
  color: #1e293b;
  margin-bottom: 4px;
}
.stat-desc {
  font-size: 13px;
  color: #94a3b8;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

/* 信息卡片 */
.info-row {
  margin-top: 4px;
}
.info-card {
  border-radius: 14px;
  border: none;
  margin-bottom: 16px;
}
.info-card :deep(.el-card__header) {
  border-bottom: 1px solid #f1f5f9;
  padding: 14px 20px;
}
.info-card-title {
  font-size: 14px;
  font-weight: 600;
  color: #334155;
}
.info-item {
  display: flex;
  justify-content: space-between;
  padding: 10px 0;
  font-size: 14px;
  color: #475569;
  border-bottom: 1px solid #f8fafc;
}
.info-item:last-child {
  border-bottom: none;
}
.info-item .label {
  color: #94a3b8;
}

.quick-btn {
  margin: 0 8px 8px 0;
  border-radius: 8px;
}
</style>
