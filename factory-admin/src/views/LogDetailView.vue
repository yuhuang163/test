<template>
  <div v-loading="loading">
    <el-page-header @back="goBack" content="日志详情" class="mb" />

    <el-descriptions v-if="detail" :column="3" border class="mb">
      <el-descriptions-item label="工厂">{{ detail.factoryDisplayName }}</el-descriptions-item>
      <el-descriptions-item label="工站">{{ detail.station }}</el-descriptions-item>
      <el-descriptions-item label="电脑名字">{{ detail.hostName || detail.deviceId }}</el-descriptions-item>
      <el-descriptions-item label="上传时间">{{ formatTime(detail.createdAt) }}</el-descriptions-item>
      <el-descriptions-item label="SN">{{ detail.sn || '-' }}</el-descriptions-item>
      <el-descriptions-item label="MAC">{{ detail.mac || '-' }}</el-descriptions-item>
      <el-descriptions-item label="结果">
        <span :class="testResultClass(detail.testResult)">
          {{ detail.testResult || '-' }}
        </span>
      </el-descriptions-item>
      <el-descriptions-item label="版本">{{ detail.clientVersion || '-' }}</el-descriptions-item>
      <el-descriptions-item label="操作">
        <el-button size="small" @click="downloadZip">下载完整 zip</el-button>
      </el-descriptions-item>
    </el-descriptions>

    <el-row :gutter="16">
      <el-col :span="8">
        <el-card header="文件列表">
          <el-tree
            :data="treeData"
            :props="{ label: 'label', children: 'children' }"
            highlight-current
            default-expand-all
            @node-click="onFileClick"
          />
        </el-card>
      </el-col>
      <el-col :span="16">
        <el-card :header="currentFile || '预览'">
          <LogPreview :text="previewText" />
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { computed, onMounted, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'
import http from '../api/http'
import { fetchLogPreviewText } from '../api/logs'
import LogPreview from '../components/LogPreview.vue'
import { formatTime, formatSize, testResultClass } from '../utils/format'

const route = useRoute()
const router = useRouter()
const loading = ref(false)
const detail = ref(null)
const previewText = ref('请选择左侧文本文件预览')
const currentFile = ref('')

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

const treeData = computed(() => buildFileTree(detail.value?.files))

function goBack() {
  router.push('/data/logs')
}

async function load() {
  loading.value = true
  try {
    detail.value = await http.get(`/logs/${route.params.id}`)
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    loading.value = false
  }
}

async function onFileClick(node) {
  if (!node.path) {
    return
  }
  if (!node.previewable) {
    previewText.value = '该文件不支持在线预览，请下载 zip'
    currentFile.value = node.path
    return
  }
  currentFile.value = node.path
  try {
    previewText.value = '加载中…'
    previewText.value = await fetchLogPreviewText(route.params.id, node.path)
  } catch (e) {
    previewText.value = '预览失败'
    ElMessage.error(e.message)
  }
}

async function downloadZip() {
  const id = route.params.id
  const res = await fetch(`/api/factory-tool/logs/${id}/download`, {
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
  a.download = `log_${id}.zip`
  a.click()
  URL.revokeObjectURL(url)
}

onMounted(load)
</script>

<style scoped>
.mb { margin-bottom: 16px; }
</style>
