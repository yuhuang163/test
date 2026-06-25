<template>
  <div class="page">
    <div class="toolbar">
      <el-button @click="loadVersions">刷新</el-button>
      <router-link to="/config/test-cases">
        <el-button>返回编辑器</el-button>
      </router-link>
    </div>

    <div class="main">
      <el-card class="version-list-panel" shadow="never">
        <template #header>版本列表</template>
        <div v-loading="loading" class="version-scroll">
          <div
            v-for="v in versions"
            :key="v.version"
            class="version-item"
            :class="{
              'is-from': fromVersion === v.version,
              'is-to': toVersion === v.version,
            }"
            @click="pickVersion(v.version)"
          >
            <div class="ver-badge">
              <span v-if="fromVersion === v.version" class="badge from">基</span>
              <span v-else-if="toVersion === v.version" class="badge to">比</span>
            </div>
            <div class="ver-info">
              <div class="ver-label">{{ v.version }}</div>
              <div class="ver-meta">{{ v.fileCount }} 文件 · {{ formatTime(v.createdAt) }}</div>
            </div>
          </div>
          <el-empty v-if="!loading && !versions.length" description="暂无版本记录" />
        </div>
      </el-card>

      <el-card class="diff-panel" shadow="never">
        <template #header>
          <div class="diff-header">
            <div class="diff-selectors">
              <span class="sel-label">基版本：</span>
              <el-select v-model="fromVersion" placeholder="选择旧版本" size="small" style="width:160px" @change="onVersionChange">
                <el-option v-for="v in versions" :key="v.version" :label="v.version" :value="v.version" />
              </el-select>
              <el-button size="small" circle :disabled="!fromVersion || !toVersion" @click="swapVersions">⇄</el-button>
              <span class="sel-label">比较版本：</span>
              <el-select v-model="toVersion" placeholder="选择新版本" size="small" style="width:160px" @change="onVersionChange">
                <el-option v-for="v in versions" :key="v.version" :label="v.version" :value="v.version" />
              </el-select>
            </div>
          </div>
        </template>

        <div v-if="!fromVersion || !toVersion" class="diff-hint">
          请选择两个版本来比对差异
        </div>

        <div v-else-if="diffLoading" v-loading="diffLoading" class="diff-hint">加载中...</div>

        <div v-else-if="diffResult" class="diff-body">
          <div class="diff-summary">
            <el-tag size="small" type="success">+{{ diffResult.addedCount }} 新增</el-tag>
            <el-tag size="small" type="danger">-{{ diffResult.removedCount }} 删除</el-tag>
            <el-tag size="small" type="warning">~{{ diffResult.changedCount }} 修改</el-tag>
            <span class="summary-info">{{ fromVersion }} → {{ toVersion }}</span>
          </div>

          <div v-if="changedFiles.length === 0" class="diff-hint">两个版本完全相同，无差异</div>

          <div v-for="f in changedFiles" :key="f.path" class="diff-file-item">
            <div class="diff-file-header" @click="toggleFile(f.path)">
              <span class="file-status" :class="f.status">
                {{ statusLabel(f.status) }}
              </span>
              <span class="file-path">{{ f.path }}</span>
              <el-icon v-if="f.status === 'changed'" class="expand-icon">
                {{ expandedFile === f.path ? '▾' : '▸' }}
              </el-icon>
            </div>
            <div v-if="f.status === 'changed' && expandedFile === f.path && fileDiff(f.path)" class="diff-lines-wrap">
              <div
                v-for="(line, i) in fileDiff(f.path)"
                :key="i"
                class="diff-line"
                :class="line.type"
              >
                <span class="line-prefix">{{ linePrefix(line.type) }}</span>
                <span class="line-text">{{ line.text }}</span>
              </div>
              <div v-if="!fileDiff(f.path) || fileDiff(f.path).length === 0" class="diff-empty">
                无内容差异（仅换行符或元数据变化）
              </div>
            </div>
          </div>
        </div>
      </el-card>
    </div>
  </div>
</template>

<script setup>
import { computed, ref, watch } from 'vue'
import { ElMessage } from 'element-plus'
import { formatTime } from '../../utils/format'
import * as api from '../../api/testCases'

const loading = ref(false)
const diffLoading = ref(false)
const versions = ref([])
const fromVersion = ref('')
const toVersion = ref('')
const diffResult = ref(null)
const expandedFile = ref('')

const changedFiles = computed(() => {
  if (!diffResult.value?.files) return []
  return diffResult.value.files.filter((f) => f.status !== 'unchanged')
})

function fileDiff(path) {
  return diffResult.value?.fileDiffs?.[path]?.filter((l) => l.type !== 'ctx') || null
}

async function loadVersions() {
  loading.value = true
  try {
    const data = await api.listVersions()
    versions.value = data || []
    if (versions.value.length >= 2 && !fromVersion.value) {
      fromVersion.value = versions.value[versions.value.length - 2].version
      toVersion.value = versions.value[versions.value.length - 1].version
    }
  } catch {
    versions.value = []
  } finally {
    loading.value = false
  }
}

function pickVersion(version) {
  if (!fromVersion.value || fromVersion.value === version) {
    fromVersion.value = version
  } else if (!toVersion.value || toVersion.value === version || version === fromVersion.value) {
    toVersion.value = version
  } else {
    fromVersion.value = toVersion.value
    toVersion.value = version
  }
}

function onVersionChange() {
  expandedFile.value = ''
  if (fromVersion.value && toVersion.value && fromVersion.value !== toVersion.value) {
    loadDiff()
  }
}

watch([fromVersion, toVersion], () => {
  if (fromVersion.value && toVersion.value && fromVersion.value !== toVersion.value) {
    expandedFile.value = ''
    diffResult.value = null
    loadDiff()
  }
})

async function loadDiff() {
  diffLoading.value = true
  try {
    const data = await api.diffVersions(fromVersion.value, toVersion.value)
    diffResult.value = data
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    diffLoading.value = false
  }
}

function swapVersions() {
  const tmp = fromVersion.value
  fromVersion.value = toVersion.value
  toVersion.value = tmp
}

function toggleFile(path) {
  expandedFile.value = expandedFile.value === path ? '' : path
}

function statusLabel(status) {
  return { added: '新增', removed: '删除', changed: '修改' }[status] || ''
}

function linePrefix(type) {
  return { add: '+', del: '-' }[type] || ' '
}

loadVersions()
</script>

<style scoped>
.page { height: calc(100vh - 120px); display: flex; flex-direction: column; }
.toolbar { margin-bottom: 12px; display: flex; align-items: center; gap: 8px; }
.main { flex: 1; display: flex; gap: 12px; min-height: 0; }

.version-list-panel { width: 240px; flex-shrink: 0; overflow: hidden; display: flex; flex-direction: column; }
.version-list-panel :deep(.el-card__body) { flex: 1; overflow: hidden; padding: 0; }
.version-scroll { height: 100%; overflow-y: auto; padding: 8px 0; }

.version-item {
  display: flex; align-items: center; gap: 8px;
  padding: 8px 12px; cursor: pointer;
  border-left: 3px solid transparent;
  transition: background 0.15s;
}
.version-item:hover { background: #f5f7fa; }
.version-item.is-from { border-left-color: #409eff; background: #ecf5ff; }
.version-item.is-to { border-left-color: #67c23a; background: #f0f9eb; }

.ver-badge { width: 20px; flex-shrink: 0; text-align: center; }
.badge {
  display: inline-block; width: 20px; height: 20px; line-height: 20px;
  border-radius: 4px; font-size: 11px; text-align: center; color: #fff; font-weight: 600;
}
.badge.from { background: #409eff; }
.badge.to { background: #67c23a; }

.ver-info { flex: 1; min-width: 0; }
.ver-label { font-weight: 600; font-size: 13px; font-family: Consolas, monospace; }
.ver-meta { font-size: 11px; color: #999; margin-top: 2px; }

.diff-panel { flex: 1; min-width: 0; display: flex; flex-direction: column; overflow: hidden; }
.diff-panel :deep(.el-card__body) { flex: 1; overflow-y: auto; padding: 16px; }

.diff-header { display: flex; align-items: center; width: 100%; }
.diff-selectors { display: flex; align-items: center; gap: 6px; flex-wrap: wrap; }
.sel-label { font-size: 13px; color: #666; white-space: nowrap; }

.diff-hint { display: flex; align-items: center; justify-content: center; height: 200px; color: #999; font-size: 14px; }

.diff-summary { display: flex; align-items: center; gap: 8px; margin-bottom: 16px; flex-wrap: wrap; padding-bottom: 12px; border-bottom: 1px solid #eee; }
.summary-info { margin-left: auto; font-size: 12px; color: #999; font-family: Consolas, monospace; }

.diff-file-item { margin-bottom: 8px; border: 1px solid #e8e8e8; border-radius: 4px; overflow: hidden; }
.diff-file-header {
  display: flex; align-items: center; gap: 8px;
  padding: 8px 12px; cursor: pointer; font-size: 13px;
  background: #fafafa; user-select: none;
}
.diff-file-header:hover { background: #f0f0f0; }

.file-status {
  font-size: 11px; padding: 2px 8px; border-radius: 3px;
  flex-shrink: 0; font-weight: 600; min-width: 36px; text-align: center;
}
.file-status.added { background: #f0f9eb; color: #67c23a; }
.file-status.removed { background: #fef0f0; color: #f56c6c; }
.file-status.changed { background: #fdf6ec; color: #e6a23c; }

.file-path { font-family: Consolas, monospace; flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }
.expand-icon { color: #999; font-size: 12px; font-style: normal; }

.diff-lines-wrap { border-top: 1px solid #e8e8e8; }
.diff-line {
  display: flex; font-family: Consolas, monospace; font-size: 13px; line-height: 1.6;
  padding: 0 12px;
}
.diff-line.add { background: #e6ffed; }
.diff-line.del { background: #ffeef0; }
.line-prefix { width: 20px; flex-shrink: 0; user-select: none; font-weight: 700; }
.line-text { white-space: pre-wrap; word-break: break-all; }
.diff-line.add .line-prefix { color: #28a745; }
.diff-line.del .line-prefix { color: #d73a49; }

.diff-empty { padding: 16px; text-align: center; color: #999; font-size: 13px; }
</style>
