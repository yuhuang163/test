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
        <el-select v-model="filters.testResult" clearable placeholder="全部" style="width: 110px">
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
      <el-table-column prop="createdAt" label="上传时间" width="180">
        <template #default="{ row }">{{ formatTime(row.createdAt) }}</template>
      </el-table-column>
      <el-table-column prop="station" label="工站" width="120" />
      <el-table-column label="电脑名字" width="180">
        <template #default="{ row }">{{ row.hostName || row.deviceId }}</template>
      </el-table-column>
      <el-table-column prop="sn" label="SN" width="140" show-overflow-tooltip />
      <el-table-column prop="mac" label="MAC" width="150" show-overflow-tooltip />
      <el-table-column label="结果" width="80">
        <template #default="{ row }">
          <span :class="testResultClass(row.testResult)">
            {{ row.testResult || '-' }}
          </span>
        </template>
      </el-table-column>
      <el-table-column label="大小" width="100">
        <template #default="{ row }">{{ formatSize(row.size) }}</template>
      </el-table-column>
      <el-table-column label="文件数" prop="fileCount" width="80" />
      <el-table-column label="操作" fixed="right" width="160">
        <template #default="{ row }">
          <el-button link type="primary" @click="goDetail(row.id)">查看</el-button>
          <el-button link @click="download(row.id)">下载</el-button>
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
  </div>
</template>

<script setup>
import { onMounted, reactive, ref } from 'vue'
import { useRouter } from 'vue-router'
import { ElMessage } from 'element-plus'
import { formatTime, formatSize, testResultClass } from '../utils/format'
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

async function loadFactories() {
  factories.value = await http.get('/admin/meta/factories')
}

async function load() {
  loading.value = true
  try {
    const data = await http.get('/logs', {
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

function goDetail(id) {
  router.push(`/data/logs/${id}`)
}

async function download(id) {
  try {
    const res = await fetch(`/api/factory-tool/logs/${id}/download`, {
      headers: { Authorization: `Bearer ${localStorage.getItem('fc_token')}` },
    })
    if (!res.ok) throw new Error('下载失败')
    const blob = await res.blob()
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `log_${id}.zip`
    a.click()
    URL.revokeObjectURL(url)
  } catch (e) {
    ElMessage.error(e.message)
  }
}

onMounted(async () => {
  await loadFactories()
  applyScopedFactoryFilter(filters)
  await load()
})
</script>

<style scoped>
.filter { margin-bottom: 16px; }
.pager { margin-top: 16px; justify-content: flex-end; }
</style>
