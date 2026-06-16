<template>
  <div>
    <div class="toolbar">
      <el-button type="primary" @click="goNew">新建模板</el-button>
      <el-button @click="load">刷新</el-button>
    </div>

    <el-table :data="items" v-loading="loading" border>
      <el-table-column prop="name" label="模板名" min-width="140" />
      <el-table-column prop="stationKey" label="工站" width="120" />
      <el-table-column prop="productModel" label="产品型号" width="120" />
      <el-table-column prop="version" label="已发布版本" width="110" />
      <el-table-column prop="status" label="状态" width="100">
        <template #default="{ row }">
          <el-tag :type="row.status === 'published' ? 'success' : 'info'" size="small">
            {{ row.status === 'published' ? '已发布' : '草稿' }}
          </el-tag>
        </template>
      </el-table-column>
      <el-table-column prop="updatedAt" label="更新时间" width="180">
        <template #default="{ row }">{{ formatTime(row.updatedAt) }}</template>
      </el-table-column>
      <el-table-column label="操作" fixed="right" width="200">
        <template #default="{ row }">
          <el-button link type="primary" @click="goEdit(row.id)">编辑</el-button>
          <el-button
            link
            type="success"
            :disabled="row.status === 'published'"
            @click="onPublish(row)"
          >发布</el-button>
        </template>
      </el-table-column>
    </el-table>

    <el-empty v-if="!loading && !items.length" description="暂无阈值模板">
      <el-button type="primary" @click="goNew">去创建</el-button>
    </el-empty>
  </div>
</template>

<script setup>
import { onMounted, ref } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage, ElMessageBox } from 'element-plus'
import { formatTime } from '../../utils/format'
import * as api from '../../api/thresholds'

const router = useRouter()
const loading = ref(false)
const items = ref([])

async function load() {
  loading.value = true
  try {
    const data = await api.listTemplates()
    items.value = data?.items || data || []
  } catch {
    items.value = []
  } finally {
    loading.value = false
  }
}

function goNew() {
  router.push('/config/thresholds/new')
}

function goEdit(id) {
  router.push(`/config/thresholds/${id}`)
}

async function onPublish(row) {
  try {
    await ElMessageBox.confirm(`确认发布模板「${row.name}」？将生成新版本。`, '发布确认', { type: 'warning' })
    const data = await api.publishTemplate(row.id)
    ElMessage.success(`发布成功，版本 ${data?.version || ''}`)
    await load()
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  }
}

onMounted(load)
</script>

<style scoped>
.toolbar { margin-bottom: 16px; display: flex; gap: 8px; }
</style>
