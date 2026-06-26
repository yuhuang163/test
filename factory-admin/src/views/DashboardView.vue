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
              <div class="quick-icon" :style="{ background: link.bg }">
                <el-icon :size="22" color="#fff"><component :is="link.icon" /></el-icon>
              </div>
              <div class="quick-title">{{ link.title }}</div>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16" class="section-row">
      <el-col :span="12">
        <el-card shadow="never" class="section-card">
          <template #header>
            <div class="section-header">
              <span>最近测试记录</span>
              <el-button size="small" text @click="go('/data/test-records')">查看全部</el-button>
            </div>
          </template>
          <el-table :data="summary.recentRecords" size="small" v-if="summary.recentRecords?.length" @row-click="(r) => go('/data/test-records')">
            <el-table-column label="时间" width="140">
              <template #default="{ row }">{{ formatTime(row.testedAt) }}</template>
            </el-table-column>
            <el-table-column prop="sn" label="SN" width="130" />
            <el-table-column prop="station" label="工站" width="100" />
            <el-table-column prop="testResult" label="结果" width="70">
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
      <el-col :span="12">
        <el-card shadow="never" class="section-card">
          <template #header>
            <div class="section-header">
              <span>最近日志上传</span>
              <el-button size="small" text @click="go('/data/logs')">查看全部</el-button>
            </div>
          </template>
          <el-table :data="summary.recentLogs" size="small" v-if="summary.recentLogs?.length" @row-click="() => go('/data/logs')">
            <el-table-column label="时间" width="140">
              <template #default="{ row }">{{ formatTime(row.createdAt) }}</template>
            </el-table-column>
            <el-table-column prop="hostName" label="电脑" width="130" />
            <el-table-column prop="station" label="工站" width="140" />
          </el-table>
          <el-empty v-else description="暂无日志" :image-size="80" />
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
import * as api from '../api/analytics'
import {
  Document, List, CircleCheck, TrendCharts,
  Files, Tools, Upload, UserFilled, Clock, DataAnalysis, DataLine
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
  { title: '日志查询', path: '/data/logs', icon: Document, bg: 'linear-gradient(135deg, #3b82f6, #2563eb)' },
  { title: '测试数据', path: '/data/test-records', icon: List, bg: 'linear-gradient(135deg, #8b5cf6, #7c3aed)' },
  { title: '数据曲线', path: '/data/curve', icon: DataLine, bg: 'linear-gradient(135deg, #10b981, #059669)' },
  { title: '良率统计', path: '/data/yield', icon: TrendCharts, bg: 'linear-gradient(135deg, #f59e0b, #d97706)' },
]

if (isEngineer.value) {
  quickLinks.push({ title: '测试用例', path: '/config/test-cases', icon: Files, bg: 'linear-gradient(135deg, #ec4899, #db2777)' })
}
if (isAdmin.value) {
  quickLinks.push({ title: '上位机版本', path: '/config/host-app', icon: Upload, bg: 'linear-gradient(135deg, #14b8a6, #0d9488)' })
  quickLinks.push({ title: '账号管理', path: '/system/users', icon: UserFilled, bg: 'linear-gradient(135deg, #f97316, #ea580c)' })
  quickLinks.push({ title: '设备登记', path: '/system/devices', icon: Tools, bg: 'linear-gradient(135deg, #6366f1, #4f46e5)' })
  quickLinks.push({ title: '登录审计', path: '/system/audit-logins', icon: Clock, bg: 'linear-gradient(135deg, #78716c, #57534e)' })
}

const roleText = computed(() => {
  const r = user.roles || []
  if (!r.length) return '未分配'
  const map = { admin: '管理员', engineer: '工艺工程师', operator: '产线操作员' }
  return r.map((x) => map[x] || x).join('、')
})

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
.dashboard { max-width: 1200px; }

.welcome-card {
  background: linear-gradient(135deg, #1e293b, #0f172a);
  border-radius: 16px;
  padding: 24px 32px;
  margin-bottom: 20px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  color: #fff;
}
.welcome-text h3 { margin: 0 0 4px; font-size: 20px; font-weight: 700; }
.welcome-text p { margin: 0; color: rgba(255,255,255,0.55); font-size: 14px; }
.welcome-time { font-size: 15px; color: rgba(255,255,255,0.5); font-variant-numeric: tabular-nums; }

.stat-card {
  margin-bottom: 16px;
  border-radius: 14px;
  border: none;
  display: flex;
  align-items: center;
  padding: 18px 20px;
  background: #fff;
}
.stat-icon {
  width: 52px; height: 52px;
  border-radius: 14px;
  display: flex; align-items: center; justify-content: center;
  flex-shrink: 0; margin-right: 16px;
}
.blue .stat-icon { background: linear-gradient(135deg, #3b82f6, #2563eb); }
.green .stat-icon { background: linear-gradient(135deg, #10b981, #059669); }
.teal .stat-icon { background: linear-gradient(135deg, #14b8a6, #0d9488); }
.orange .stat-icon { background: linear-gradient(135deg, #f59e0b, #d97706); }
.red .stat-icon { background: linear-gradient(135deg, #ef4444, #dc2626); }

.stat-body { flex: 1; min-width: 0; }
.stat-val { font-size: 30px; font-weight: 700; color: #1e293b; line-height: 1.2; }
.stat-label { font-size: 13px; color: #94a3b8; margin-top: 2px; }

.section-row { margin-top: 4px; }
.section-card { border-radius: 14px; border: none; margin-bottom: 16px; }
.section-header { display: flex; align-items: center; justify-content: space-between; font-weight: 600; font-size: 14px; color: #334155; }

.factory-list { display: flex; flex-direction: column; gap: 12px; }
.factory-item { display: flex; align-items: center; gap: 12px; }
.factory-name { width: 80px; font-size: 13px; color: #475569; flex-shrink: 0; }
.factory-count { width: 40px; text-align: right; font-size: 13px; font-weight: 600; color: #1e293b; flex-shrink: 0; }

.quick-grid { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px; }
.quick-item {
  display: flex; flex-direction: column; align-items: center;
  gap: 8px; cursor: pointer; padding: 16px 8px;
  border-radius: 12px; transition: all 0.2s;
}
.quick-item:hover { background: #f8fafc; transform: translateY(-2px); }
.quick-icon {
  width: 46px; height: 46px; border-radius: 12px;
  display: flex; align-items: center; justify-content: center;
}
.quick-title { font-size: 12px; color: #475569; font-weight: 500; text-align: center; }

:deep(.el-table) { cursor: pointer; }
:deep(.el-empty) { padding: 20px 0; }
</style>
