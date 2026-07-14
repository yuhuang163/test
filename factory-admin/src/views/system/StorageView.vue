<template>
  <div class="storage-page" v-loading="loading">
    <div class="toolbar">
      <el-button type="primary" :loading="loading" @click="load">刷新</el-button>
      <span class="hint">统计耗时取决于日志目录大小，可稍后再次刷新</span>
    </div>

    <el-alert
      v-if="info?.alert"
      class="alert-box"
      :title="info.alert.message"
      :type="alertType"
      :closable="false"
      show-icon
    />

    <el-row :gutter="16" class="cards">
      <el-col :xs="24" :md="12">
        <el-card shadow="never" class="panel">
          <template #header>
            <div class="panel-title">盘符容量</div>
          </template>
          <div v-if="info?.disk" class="disk-block">
            <div class="disk-mount">{{ info.disk.mount }}</div>
            <div class="disk-path">{{ info.disk.path }}</div>
            <el-progress
              :percentage="Math.min(100, Number(info.disk.usedPercent) || 0)"
              :status="progressStatus"
              :stroke-width="14"
              :format="(p) => `${Number(p).toFixed(1)}%`"
            />
            <div class="disk-stats">
              <div><span>总容量</span><b>{{ formatBytes(info.disk.totalBytes) }}</b></div>
              <div><span>已用</span><b>{{ formatBytes(info.disk.usedBytes) }}</b></div>
              <div><span>剩余</span><b>{{ formatBytes(info.disk.freeBytes) }}</b></div>
            </div>
            <div class="threshold">
              预警 {{ info.thresholds?.warnPercent ?? 80 }}% · 紧急 {{ info.thresholds?.criticalPercent ?? 90 }}%
            </div>
          </div>
          <el-empty v-else description="暂无磁盘信息" :image-size="60" />
        </el-card>
      </el-col>

      <el-col :xs="24" :md="12">
        <el-card shadow="never" class="panel">
          <template #header>
            <div class="panel-title">占用概览</div>
          </template>
          <div class="overview-list">
            <div class="overview-item">
              <span>存储根目录</span>
              <b>{{ formatBytes(info?.storageRoot?.sizeBytes) }}</b>
            </div>
            <div class="overview-item muted">{{ info?.storageRoot?.path || '-' }}</div>
            <div class="overview-item">
              <span>日志占用</span>
              <b class="em">{{ formatBytes(logsSize) }}</b>
            </div>
            <div class="overview-item">
              <span>测试数据占用</span>
              <b class="em">{{ formatBytes(testDataSize) }}</b>
            </div>
            <div class="overview-item">
              <span>数据库文件</span>
              <b>{{ formatBytes(info?.database?.sizeBytes) }}</b>
            </div>
            <div class="overview-item muted">{{ info?.database?.path || '-' }}</div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <el-card shadow="never" class="panel">
      <template #header>
        <div class="panel-title">分类明细</div>
      </template>
      <el-table :data="info?.breakdown || []" stripe>
        <el-table-column prop="label" label="类别" width="140" />
        <el-table-column prop="sizeBytes" label="占用" width="140">
          <template #default="{ row }">{{ formatBytes(row.sizeBytes) }}</template>
        </el-table-column>
        <el-table-column prop="path" label="路径" min-width="280" show-overflow-tooltip />
        <el-table-column prop="hint" label="说明" width="200">
          <template #default="{ row }">{{ row.hint || '-' }}</template>
        </el-table-column>
      </el-table>
    </el-card>

    <el-card v-if="info?.databaseDisk" shadow="never" class="panel">
      <template #header>
        <div class="panel-title">数据库所在盘（与存储目录不同盘）</div>
      </template>
      <div class="disk-block">
        <div class="disk-mount">{{ info.databaseDisk.mount }}</div>
        <div class="disk-path">{{ info.databaseDisk.path }}</div>
        <el-progress
          :percentage="Math.min(100, Number(info.databaseDisk.usedPercent) || 0)"
          :stroke-width="12"
          :format="(p) => `${Number(p).toFixed(1)}%`"
        />
        <div class="disk-stats">
          <div><span>总容量</span><b>{{ formatBytes(info.databaseDisk.totalBytes) }}</b></div>
          <div><span>已用</span><b>{{ formatBytes(info.databaseDisk.usedBytes) }}</b></div>
          <div><span>剩余</span><b>{{ formatBytes(info.databaseDisk.freeBytes) }}</b></div>
        </div>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { ElMessage } from 'element-plus'
import * as api from '../../api/storage'

const loading = ref(false)
const info = ref(null)

const alertType = computed(() => {
  const level = info.value?.alert?.level
  if (level === 'critical') return 'error'
  if (level === 'warn') return 'warning'
  return 'success'
})

const progressStatus = computed(() => {
  const level = info.value?.alert?.level
  if (level === 'critical') return 'exception'
  if (level === 'warn') return 'warning'
  return undefined
})

const logsSize = computed(() => {
  const row = (info.value?.breakdown || []).find((x) => x.key === 'logs')
  return row?.sizeBytes || 0
})

const testDataSize = computed(() => {
  const row = (info.value?.breakdown || []).find((x) => x.key === 'testData')
  return row?.sizeBytes || 0
})

function formatBytes(n) {
  const num = Number(n)
  if (!Number.isFinite(num) || num < 0) return '-'
  const units = ['B', 'KB', 'MB', 'GB', 'TB']
  let v = num
  for (let i = 0; i < units.length; i++) {
    if (v < 1024 || i === units.length - 1) {
      return i === 0 ? `${Math.round(v)} ${units[i]}` : `${v.toFixed(2)} ${units[i]}`
    }
    v /= 1024
  }
  return `${num} B`
}

async function load() {
  loading.value = true
  try {
    info.value = await api.getStorageInfo()
  } catch (e) {
    info.value = null
    ElMessage.error(e.message || '加载存储信息失败')
  } finally {
    loading.value = false
  }
}

onMounted(load)
</script>

<style scoped>
.toolbar {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 16px;
}
.hint {
  font-size: 12px;
  color: var(--admin-text-tertiary);
}
.alert-box {
  margin-bottom: 16px;
}
.cards {
  margin-bottom: 16px;
}
.panel {
  margin-bottom: 16px;
  border-radius: var(--admin-radius-lg);
}
.panel-title {
  font-weight: 600;
  color: var(--admin-text);
}
.disk-block .disk-mount {
  font-size: 22px;
  font-weight: 700;
  color: var(--admin-text);
}
.disk-block .disk-path {
  font-size: 12px;
  color: var(--admin-text-tertiary);
  margin: 4px 0 14px;
  word-break: break-all;
}
.disk-stats {
  display: flex;
  gap: 20px;
  margin-top: 14px;
  flex-wrap: wrap;
}
.disk-stats span {
  display: block;
  font-size: 12px;
  color: var(--admin-text-tertiary);
}
.disk-stats b {
  font-size: 15px;
  color: var(--admin-text);
}
.threshold {
  margin-top: 10px;
  font-size: 12px;
  color: var(--admin-text-tertiary);
}
.overview-list {
  display: flex;
  flex-direction: column;
  gap: 10px;
}
.overview-item {
  display: flex;
  justify-content: space-between;
  gap: 12px;
  font-size: 14px;
  color: var(--admin-text);
}
.overview-item.muted {
  display: block;
  font-size: 12px;
  color: var(--admin-text-tertiary);
  word-break: break-all;
  margin-top: -6px;
}
.overview-item .em {
  color: var(--admin-primary);
}
</style>
