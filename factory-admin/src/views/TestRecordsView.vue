<template>
  <div>
    <el-form :inline="true" class="filter">
      <el-form-item v-if="isFactoryScoped" label="工厂">
        <el-input :model-value="scopedFactoryLabel" disabled style="width: 140px" />
      </el-form-item>
      <el-form-item v-else label="工厂">
        <el-select v-model="filters.factoryName" clearable placeholder="全部" style="width: 140px">
          <el-option v-for="f in factories" :key="f.code" :label="f.displayName" :value="f.code" />
        </el-select>
      </el-form-item>
      <el-form-item label="工站">
        <el-input v-model="filters.station" clearable placeholder="工站（模糊）" style="width: 140px" />
      </el-form-item>
      <el-form-item label="电脑名字">
        <el-input v-model="filters.hostName" clearable placeholder="电脑名" style="width: 160px" />
      </el-form-item>
      <el-form-item label="SN">
        <el-input v-model="filters.sn" clearable placeholder="SN" style="width: 140px" />
      </el-form-item>
      <el-form-item label="MAC">
        <el-input v-model="filters.mac" clearable placeholder="MAC" style="width: 160px" />
      </el-form-item>
      <el-form-item label="结果">
        <el-select v-model="filters.testResult" clearable placeholder="全部" style="width: 100px">
          <el-option label="PASS" value="PASS" />
          <el-option label="NG" value="NG" />
          <el-option label="ABORT" value="ABORT" />
          <el-option label="FAIL" value="FAIL" />
          <el-option label="通过" value="通过" />
          <el-option label="失败" value="失败" />
        </el-select>
      </el-form-item>
      <el-form-item>
        <el-button type="primary" @click="load">查询</el-button>
      </el-form-item>
    </el-form>

    <el-table :data="items" v-loading="loading">
      <el-table-column prop="factoryDisplayName" label="工厂" width="100" />
      <el-table-column label="测试时间" width="180">
        <template #default="{ row }">{{ formatTime(row.testedAt || row.createdAt) }}</template>
      </el-table-column>
      <el-table-column prop="station" label="工站" width="120" />
      <el-table-column label="电脑名字" width="160">
        <template #default="{ row }">{{ row.hostName || row.deviceId }}</template>
      </el-table-column>
      <el-table-column prop="sn" label="SN" width="160" show-overflow-tooltip />
      <el-table-column prop="mac" label="MAC" width="150" show-overflow-tooltip />
      <el-table-column label="结果" width="80">
        <template #default="{ row }">
          <span :class="{ 'result-fail': isTestFailResult(row.testResult) }">
            {{ row.testResult || '-' }}
          </span>
        </template>
      </el-table-column>
      <el-table-column prop="product" label="产品" width="100" />
      <el-table-column prop="itemCount" label="分项数" width="80" />
      <el-table-column label="操作" fixed="right" width="100">
        <template #default="{ row }">
          <el-button link type="primary" @click="openDetail(row.id)">查看</el-button>
        </template>
      </el-table-column>
    </el-table>

    <el-pagination
      class="pager"
      background
      layout="total, prev, pager, next"
      :total="total"
      :page-size="pageSize"
      v-model:current-page="page"
      @current-change="load"
    />

    <el-drawer v-model="drawerVisible" title="测试数据详情" size="70%">
      <div v-if="detail" v-loading="detailLoading">
        <el-descriptions :column="2" border class="mb">
          <el-descriptions-item label="工厂">{{ detail.factoryDisplayName }}</el-descriptions-item>
          <el-descriptions-item label="工站">{{ detail.station }}</el-descriptions-item>
          <el-descriptions-item label="电脑名字">{{ detail.hostName || detail.deviceId }}</el-descriptions-item>
          <el-descriptions-item label="SN">{{ detail.sn || '-' }}</el-descriptions-item>
          <el-descriptions-item label="MAC">{{ detail.mac || '-' }}</el-descriptions-item>
          <el-descriptions-item label="结果">
            <span :class="{ 'result-fail': isTestFailResult(detail.testResult) }">
              {{ detail.testResult || '-' }}
            </span>
          </el-descriptions-item>
          <el-descriptions-item label="产品">{{ detail.product || '-' }}</el-descriptions-item>
          <el-descriptions-item label="工单">{{ detail.lotName || '-' }}</el-descriptions-item>
          <el-descriptions-item label="操作员">{{ detail.userNo || '-' }}</el-descriptions-item>
          <el-descriptions-item label="版本">{{ detail.clientVersion || '-' }}</el-descriptions-item>
          <el-descriptions-item label="测试时间">{{ formatTime(detail.testedAt || detail.createdAt) }}</el-descriptions-item>
        </el-descriptions>

        <el-tabs v-model="detailTab" class="mb">
          <el-tab-pane label="测试分项" name="items">
            <el-table :data="detail.items" size="small">
              <el-table-column prop="name" label="测试项" min-width="140" />
              <el-table-column prop="value" label="实测值" width="100" />
              <el-table-column prop="minValue" label="下限" width="80" />
              <el-table-column prop="maxValue" label="上限" width="80" />
              <el-table-column prop="standardValue" label="标准值" width="80" />
              <el-table-column prop="unit" label="单位" width="60" />
              <el-table-column label="结果" width="80">
                <template #default="{ row }">
                  <span :class="{ 'result-fail': isTestFailResult(row.result) }">
                    {{ row.result || '-' }}
                  </span>
                </template>
              </el-table-column>
            </el-table>
          </el-tab-pane>

          <el-tab-pane label="测试日志" name="logs">
            <div v-if="detail.logArchive" class="log-pane">
              <div class="log-actions mb">
                <el-button size="small" type="primary" @click="downloadSessionLog">下载本次日志 zip</el-button>
                <el-button size="small" @click="openInLogLibrary">在日志库中打开</el-button>
              </div>
              <el-row :gutter="16">
                <el-col :span="8">
                  <el-card header="文件列表" shadow="never">
                    <el-tree
                      :data="logTreeData"
                      :props="{ label: 'label', children: 'children' }"
                      highlight-current
                      default-expand-all
                      @node-click="onLogFileClick"
                    />
                  </el-card>
                </el-col>
                <el-col :span="16">
                  <el-card :header="currentLogFile || '预览'" shadow="never">
                    <pre class="preview">{{ logPreviewText }}</pre>
                  </el-card>
                </el-col>
              </el-row>
            </div>
            <el-empty v-else description="暂无关联日志">
              <el-button type="primary" link @click="searchLogsBySn">按 SN 搜索日志库</el-button>
            </el-empty>
          </el-tab-pane>
        </el-tabs>
      </div>
    </el-drawer>
  </div>
</template>

<script setup>
import { computed, onMounted, reactive, ref, watch } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'
import { formatTime, formatSize, isTestFailResult } from '../utils/format'
import { useFactoryScope } from '../composables/useFactoryScope'
import http from '../api/http'

const router = useRouter()
const { isFactoryScoped, scopedFactoryLabel, applyScopedFactoryFilter } = useFactoryScope()
const loading = ref(false)
const items = ref([])
const total = ref(0)
const page = ref(1)
const pageSize = 20
const factories = ref([])
const filters = reactive({ factoryName: '', station: '', hostName: '', sn: '', mac: '', testResult: '' })

const drawerVisible = ref(false)
const detailLoading = ref(false)
const detail = ref(null)
const detailTab = ref('items')
const logPreviewText = ref('请选择左侧文本文件预览')
const currentLogFile = ref('')

function buildFileTree(files) {
  const root = []
  const folderMap = new Map()

  const ensureFolder = (parts, index) => {
    const key = parts.slice(0, index + 1).join('/')
    if (folderMap.has(key)) {
      return folderMap.get(key)
    }
    const node = {
      label: parts[index],
      children: [],
    }
    folderMap.set(key, node)
    if (index === 0) {
      root.push(node)
    } else {
      const parent = ensureFolder(parts, index - 1)
      parent.children.push(node)
    }
    return node
  }

  for (const file of files || []) {
    const parts = file.relativePath.split('/').filter(Boolean)
    if (!parts.length) continue
    const parent = parts.length > 1 ? ensureFolder(parts, parts.length - 2) : null
    const leaf = {
      label: `${parts[parts.length - 1]} (${formatSize(file.size)})`,
      path: file.relativePath,
      previewable: file.previewable,
    }
    if (parent) {
      parent.children.push(leaf)
    } else {
      root.push(leaf)
    }
  }
  return root
}

const logTreeData = computed(() => buildFileTree(detail.value?.logArchive?.files))

function pickDefaultLogFile(files) {
  if (!files?.length) return null
  const sessionTxt = files.find(
    (f) => f.previewable && /上位机log\/.+\.txt$/i.test(f.relativePath)
  )
  return sessionTxt || files.find((f) => f.previewable) || null
}

async function previewLogFile(logId, relativePath) {
  currentLogFile.value = relativePath
  const res = await fetch(
    `/api/factory-tool/logs/${logId}/files/${encodeURIComponent(relativePath)}`,
    { headers: { Authorization: `Bearer ${localStorage.getItem('fc_token')}` } }
  )
  if (!res.ok) {
    throw new Error('预览失败')
  }
  logPreviewText.value = await res.text()
}

async function loadFactories() {
  factories.value = await http.get('/admin/meta/factories')
}

async function load() {
  loading.value = true
  try {
    const data = await http.get('/test-records', {
      params: {
        page: page.value,
        pageSize,
        factoryName: filters.factoryName || undefined,
        station: filters.station || undefined,
        hostName: filters.hostName || undefined,
        sn: filters.sn || undefined,
        mac: filters.mac || undefined,
        testResult: filters.testResult || undefined,
      },
    })
    items.value = data.items || []
    total.value = data.total || 0
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    loading.value = false
  }
}

async function openDetail(id) {
  drawerVisible.value = true
  detailLoading.value = true
  detail.value = null
  logPreviewText.value = '请选择左侧文本文件预览'
  currentLogFile.value = ''
  try {
    detail.value = await http.get(`/test-records/${id}`)
    detailTab.value = detail.value?.logArchive ? 'logs' : 'items'
    const defaultFile = pickDefaultLogFile(detail.value?.logArchive?.files)
    if (defaultFile) {
      await previewLogFile(detail.value.logArchive.id, defaultFile.relativePath)
    }
  } catch (e) {
    ElMessage.error(e.message)
    drawerVisible.value = false
  } finally {
    detailLoading.value = false
  }
}

async function onLogFileClick(node) {
  if (!node.path || !detail.value?.logArchive) {
    return
  }
  if (!node.previewable) {
    logPreviewText.value = '该文件不支持在线预览，请下载 zip'
    currentLogFile.value = node.path
    return
  }
  try {
    await previewLogFile(detail.value.logArchive.id, node.path)
  } catch (e) {
    ElMessage.error(e.message)
  }
}

async function downloadSessionLog() {
  const archive = detail.value?.logArchive
  if (!archive) {
    return
  }
  const res = await fetch(`/api/factory-tool/logs/${archive.id}/download`, {
    headers: { Authorization: `Bearer ${localStorage.getItem('fc_token')}` },
  })
  if (!res.ok) {
    ElMessage.error('下载失败')
    return
  }
  const blob = await res.blob()
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = `test_log_${detail.value.id}_${archive.id}.zip`
  a.click()
  URL.revokeObjectURL(url)
}

function openInLogLibrary() {
  const archive = detail.value?.logArchive
  if (!archive) {
    return
  }
  router.push(`/data/logs/${archive.id}`)
}

function searchLogsBySn() {
  const sn = detail.value?.sn
  if (sn) {
    router.push({ path: '/data/logs', query: { sn } })
  } else {
    router.push('/data/logs')
  }
}

watch(drawerVisible, (visible) => {
  if (!visible) {
    detailTab.value = 'items'
    logPreviewText.value = '请选择左侧文本文件预览'
    currentLogFile.value = ''
  }
})

onMounted(async () => {
  await loadFactories()
  applyScopedFactoryFilter(filters)
  await load()
})
</script>

<style scoped>
.filter { margin-bottom: 16px; }
.pager { margin-top: 16px; justify-content: flex-end; }
.mb { margin-bottom: 16px; }
.result-fail { color: #f56c6c; font-weight: 600; }
.log-actions { display: flex; gap: 8px; }
.preview {
  margin: 0;
  max-height: 55vh;
  overflow: auto;
  white-space: pre-wrap;
  word-break: break-all;
  font-size: 12px;
  line-height: 1.5;
  background: #1e1e1e;
  color: #d4d4d4;
  padding: 12px;
  border-radius: 4px;
}
</style>
