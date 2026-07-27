<template>
  <div class="dashboard" v-loading="loading">
    <div class="welcome-card">
      <div class="welcome-text">
        <h3>路特产线管理平台</h3>
        <p>{{ user.username }} · {{ roleText }}</p>
      </div>
      <div class="welcome-time">{{ now }}</div>
    </div>

    <el-row :gutter="16">
      <el-col :span="6">
        <el-card shadow="never" class="stat-card blue">
          <div class="stat-icon"><el-icon :size="28"><Document /></el-icon></div>
          <div class="stat-body">
            <div class="stat-val">{{ summary.totalRecords }}</div>
            <div class="stat-label">累计测试记录</div>
          </div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="never" class="stat-card green">
          <div class="stat-icon"><el-icon :size="28"><List /></el-icon></div>
          <div class="stat-body">
            <div class="stat-val">{{ summary.todayTotal }}</div>
            <div class="stat-label">今日测试数</div>
          </div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="never" class="stat-card teal">
          <div class="stat-icon"><el-icon :size="28"><CircleCheck /></el-icon></div>
          <div class="stat-body">
            <div class="stat-val">{{ summary.todayPass }}</div>
            <div class="stat-label">今日通过</div>
          </div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="never" class="stat-card" :class="summary.todayYield >= 95 ? 'teal' : summary.todayYield > 0 ? 'orange' : 'red'">
          <div class="stat-icon"><el-icon :size="28"><TrendCharts /></el-icon></div>
          <div class="stat-body">
            <div class="stat-val">{{ summary.todayYield }}%</div>
            <div class="stat-label">今日良率</div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="section-row">
      <el-col :span="12">
        <el-card shadow="never" class="section-card">
          <template #header>
            <div class="section-header">
              <span>各工厂测试量</span>
              <el-tag size="small" type="info">{{ summary.factories?.length || 0 }} 个工厂</el-tag>
            </div>
          </template>
          <div v-if="summary.factories?.length" class="factory-list">
            <div v-for="f in summary.factories" :key="f.name" class="factory-item">
              <span class="factory-name">{{ f.name }}</span>
              <el-progress :percentage="pct(f.count)" :stroke-width="14" :show-text="false" />
              <span class="factory-count">{{ f.count }}</span>
            </div>
          </div>
          <el-empty v-else description="暂无测试数据" :image-size="80" />
        </el-card>
      </el-col>
      <el-col :span="12">
        <el-card shadow="never" class="section-card">
          <template #header>
            <div class="section-header">
              <span>快速入口</span>
            </div>
          </template>
          <div class="quick-grid">
            <div v-for="link in quickLinks" :key="link.title" class="quick-item" @click="go(link.path)">
              <div class="quick-icon" :class="link.tone">
                <el-icon :size="20"><component :is="link.icon" /></el-icon>
              </div>
              <div class="quick-title">{{ link.title }}</div>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="section-row">
      <el-col :span="24">
        <el-card shadow="never" class="section-card">
          <template #header>
            <div class="section-header">
              <span>最近测试记录</span>
              <el-button size="small" text @click="go('/data/test-records')">查看全部</el-button>
            </div>
          </template>
          <el-table :data="summary.recentRecords" size="small" v-if="summary.recentRecords?.length" @row-click="(r) => go('/data/test-records')">
            <el-table-column label="时间" width="160">
              <template #default="{ row }">{{ formatTime(row.testedAt) }}</template>
            </el-table-column>
            <el-table-column prop="sn" label="SN" min-width="140" />
            <el-table-column prop="station" label="工站" min-width="120" />
            <el-table-column prop="factoryDisplayName" label="工厂" width="120" />
            <el-table-column prop="testResult" label="结果" width="90">
              <template #default="{ row }">
                <el-tag :type="row.testResult === 'PASS' || row.testResult === 'OK' ? 'success' : 'danger'" size="small">
                  {{ row.testResult || '-' }}
                </el-tag>
              </template>
            </el-table-column>
          </el-table>
          <el-empty v-else description="暂无测试记录" :image-size="80" />
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { computed, onMounted, reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { useUserStore } from '../stores/user'
import { useRole } from '../composables/useRole'
import { formatTime } from '../utils/format'
import { formatRoleLabels } from '../utils/roles'
import * as api from '../api/analytics'
import {
  Document, List, CircleCheck, TrendCharts,
  Files, Tools, Upload, UserFilled, Clock, DataLine
} from '@element-plus/icons-vue'

const router = useRouter()
const user = useUserStore()
const { isAdmin, isEngineer } = useRole()
const loading = ref(false)
const summary = reactive({
  totalRecords: 0, todayTotal: 0, todayPass: 0, todayFail: 0, todayYield: 0,
  totalLogs: 0, factories: [], recentRecords: [], recentLogs: [],
})

const now = ref('')

function updateTime() {
  const d = new Date()
  const pad = (n) => String(n).padStart(2, '0')
  now.value = `${d.getFullYear()}-${pad(d.getMonth()+1)}-${pad(d.getDate())} ${pad(d.getHours())}:${pad(d.getMinutes())}:${pad(d.getSeconds())}`
}

const maxFactoryCount = ref(0)
function pct(count) {
  return maxFactoryCount.value > 0 ? Math.round(count / maxFactoryCount.value * 100) : 0
}

const quickLinks = [
  { title: '测试数据', path: '/data/test-records', icon: List, tone: 'purple' },
  { title: '数据曲线', path: '/data/curve', icon: DataLine, tone: 'green' },
  { title: '良率统计', path: '/data/yield', icon: TrendCharts, tone: 'orange' },
]

if (isEngineer.value) {
  quickLinks.push({ title: '测试用例', path: '/config/test-cases', icon: Files, tone: 'pink' })
}
if (isAdmin.value) {
  quickLinks.push({ title: '上位机版本', path: '/config/host-app', icon: Upload, tone: 'cyan' })
  quickLinks.push({ title: '账号管理', path: '/system/users', icon: UserFilled, tone: 'blue' })
  quickLinks.push({ title: '设备登记', path: '/system/devices', icon: Tools, tone: 'purple' })
  quickLinks.push({ title: '登录审计', path: '/system/audit-logins', icon: Clock, tone: 'gray' })
}

const roleText = computed(() => formatRoleLabels(user.roles, '未分配'))

async function load() {
  loading.value = true
  try {
    const data = await api.getDashboardSummary()
    Object.assign(summary, data)
    maxFactoryCount.value = Math.max(...(data.factories || []).map((f) => f.count), 1)
  } catch {
    // ignore
  } finally {
    loading.value = false
  }
}

function go(path) {
  router.push(path)
}

onMounted(() => {
  load()
  updateTime()
  setInterval(updateTime, 1000)
})
</script>

<style scoped>
.dashboard { max-width: 1280px; }

.welcome-card {
  background: var(--admin-surface);
  border: 1px solid var(--admin-border);
  border-radius: var(--admin-radius-lg);
  padding: 20px 24px;
  margin-bottom: 16px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  box-shadow: var(--admin-shadow);
}
.welcome-text h3 { margin: 0 0 4px; font-size: 18px; font-weight: 600; color: var(--admin-text); }
.welcome-text p { margin: 0; color: var(--admin-text-secondary); font-size: 14px; }
.welcome-time { font-size: 14px; color: var(--admin-text-tertiary); font-variant-numeric: tabular-nums; }

.stat-card {
  margin-bottom: 16px;
  border-radius: var(--admin-radius-lg);
  display: flex;
  align-items: center;
  padding: 16px 18px;
  background: var(--admin-surface);
}
.stat-icon {
  width: 48px;
  height: 48px;
  border-radius: var(--admin-radius-lg);
  display: flex;
  align-items: center;
  justify-content: center;
  flex-shrink: 0;
  margin-right: 14px;
}
.blue .stat-icon { background: var(--admin-primary-light); color: var(--admin-primary); }
.green .stat-icon { background: #f6ffed; color: var(--admin-success); }
.teal .stat-icon { background: #e6fffb; color: #13c2c2; }
.orange .stat-icon { background: #fff7e6; color: var(--admin-warning); }
.red .stat-icon { background: #fff1f0; color: var(--admin-danger); }

.stat-body { flex: 1; min-width: 0; }
.stat-val { font-size: 28px; font-weight: 600; color: var(--admin-text); line-height: 1.2; }
.stat-label { font-size: 13px; color: var(--admin-text-tertiary); margin-top: 2px; }

.section-row { margin-top: 4px; }
.section-card { border-radius: var(--admin-radius-lg); margin-bottom: 16px; }
.section-header { display: flex; align-items: center; justify-content: space-between; font-weight: 600; font-size: 14px; color: var(--admin-text); }

.factory-list { display: flex; flex-direction: column; gap: 12px; }
.factory-item { display: flex; align-items: center; gap: 12px; }
.factory-name { width: 80px; font-size: 13px; color: var(--admin-text-secondary); flex-shrink: 0; }
.factory-count { width: 40px; text-align: right; font-size: 13px; font-weight: 600; color: var(--admin-text); flex-shrink: 0; }

.quick-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 8px; }
.quick-item {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 8px;
  cursor: pointer;
  padding: 14px 8px;
  border-radius: var(--admin-radius);
  transition: background 0.2s;
}
.quick-item:hover { background: var(--admin-border-light); }
.quick-icon {
  width: 40px;
  height: 40px;
  border-radius: var(--admin-radius);
  display: flex;
  align-items: center;
  justify-content: center;
}
.quick-icon.blue { background: var(--admin-primary-light); color: var(--admin-primary); }
.quick-icon.purple { background: #f9f0ff; color: #722ed1; }
.quick-icon.green { background: #f6ffed; color: var(--admin-success); }
.quick-icon.orange { background: #fff7e6; color: var(--admin-warning); }
.quick-icon.pink { background: #fff0f6; color: #eb2f96; }
.quick-icon.cyan { background: #e6fffb; color: #13c2c2; }
.quick-icon.gray { background: #f5f5f5; color: var(--admin-text-secondary); }
.quick-title { font-size: 12px; color: var(--admin-text-secondary); font-weight: 500; text-align: center; }

:deep(.el-table) { cursor: pointer; }
:deep(.el-empty) { padding: 20px 0; }
</style>
