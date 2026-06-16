<template>
  <div>
    <el-form :inline="true" class="filter">
      <el-form-item label="工厂">
        <el-select v-model="filters.factoryName" clearable placeholder="全部" style="width: 140px">
          <el-option v-for="f in factories" :key="f.code" :label="f.displayName" :value="f.code" />
        </el-select>
      </el-form-item>
      <el-form-item label="工站">
        <el-input v-model="filters.station" clearable placeholder="工站" style="width: 140px" />
      </el-form-item>
      <el-form-item label="电脑名字">
        <el-input v-model="filters.hostName" clearable placeholder="电脑名" style="width: 160px" />
      </el-form-item>
      <el-form-item label="SN">
        <el-input v-model="filters.sn" clearable placeholder="SN" style="width: 140px" />
      </el-form-item>
      <el-form-item label="结果">
        <el-select v-model="filters.testResult" clearable placeholder="全部" style="width: 100px">
          <el-option label="PASS" value="PASS" />
          <el-option label="FAIL" value="FAIL" />
          <el-option label="通过" value="通过" />
          <el-option label="失败" value="失败" />
        </el-select>
      </el-form-item>
      <el-form-item>
        <el-button type="primary" @click="load">查询</el-button>
      </el-form-item>
    </el-form>

    <el-table :data="items" v-loading="loading" border>
      <el-table-column prop="factoryDisplayName" label="工厂" width="100" />
      <el-table-column label="测试时间" width="180">
        <template #default="{ row }">{{ formatTime(row.testedAt || row.createdAt) }}</template>
      </el-table-column>
      <el-table-column prop="station" label="工站" width="120" />
      <el-table-column label="电脑名字" width="160">
        <template #default="{ row }">{{ row.hostName || row.deviceId }}</template>
      </el-table-column>
      <el-table-column prop="sn" label="SN" width="140" />
      <el-table-column prop="testResult" label="结果" width="80" />
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

    <el-drawer v-model="drawerVisible" title="测试数据详情" size="60%">
      <div v-if="detail" v-loading="detailLoading">
        <el-descriptions :column="2" border class="mb">
          <el-descriptions-item label="工厂">{{ detail.factoryDisplayName }}</el-descriptions-item>
          <el-descriptions-item label="工站">{{ detail.station }}</el-descriptions-item>
          <el-descriptions-item label="电脑名字">{{ detail.hostName || detail.deviceId }}</el-descriptions-item>
          <el-descriptions-item label="SN">{{ detail.sn || '-' }}</el-descriptions-item>
          <el-descriptions-item label="结果">{{ detail.testResult || '-' }}</el-descriptions-item>
          <el-descriptions-item label="产品">{{ detail.product || '-' }}</el-descriptions-item>
          <el-descriptions-item label="工单">{{ detail.lotName || '-' }}</el-descriptions-item>
          <el-descriptions-item label="操作员">{{ detail.userNo || '-' }}</el-descriptions-item>
          <el-descriptions-item label="版本">{{ detail.clientVersion || '-' }}</el-descriptions-item>
          <el-descriptions-item label="测试时间">{{ formatTime(detail.testedAt || detail.createdAt) }}</el-descriptions-item>
        </el-descriptions>
        <el-table :data="detail.items" border size="small">
          <el-table-column prop="name" label="测试项" min-width="140" />
          <el-table-column prop="value" label="实测值" width="100" />
          <el-table-column prop="minValue" label="下限" width="80" />
          <el-table-column prop="maxValue" label="上限" width="80" />
          <el-table-column prop="standardValue" label="标准值" width="80" />
          <el-table-column prop="unit" label="单位" width="60" />
          <el-table-column prop="result" label="结果" width="80" />
        </el-table>
      </div>
    </el-drawer>
  </div>
</template>

<script setup>
import { onMounted, reactive, ref } from 'vue'
import { ElMessage } from 'element-plus'
import { formatTime } from '../utils/format'
import http from '../api/http'

const loading = ref(false)
const items = ref([])
const total = ref(0)
const page = ref(1)
const pageSize = 20
const factories = ref([])
const filters = reactive({ factoryName: '', station: '', hostName: '', sn: '', testResult: '' })

const drawerVisible = ref(false)
const detailLoading = ref(false)
const detail = ref(null)

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
  try {
    detail.value = await http.get(`/test-records/${id}`)
  } catch (e) {
    ElMessage.error(e.message)
    drawerVisible.value = false
  } finally {
    detailLoading.value = false
  }
}

onMounted(async () => {
  await loadFactories()
  await load()
})
</script>

<style scoped>
.filter { margin-bottom: 16px; }
.pager { margin-top: 16px; justify-content: flex-end; }
.mb { margin-bottom: 16px; }
</style>
