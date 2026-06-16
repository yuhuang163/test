<template>
  <div>
    <el-form :inline="true" class="filter">
      <el-form-item label="用户名">
        <el-input v-model="filters.username" clearable placeholder="用户名" />
      </el-form-item>
      <el-form-item label="电脑名">
        <el-input v-model="filters.hostName" clearable placeholder="hostName" />
      </el-form-item>
      <el-form-item>
        <el-button type="primary" @click="load">查询</el-button>
      </el-form-item>
    </el-form>

    <el-table :data="items" v-loading="loading" border>
      <el-table-column prop="createdAt" label="时间" width="180">
        <template #default="{ row }">{{ formatTime(row.createdAt) }}</template>
      </el-table-column>
      <el-table-column prop="username" label="用户名" width="120" />
      <el-table-column prop="hostName" label="电脑名" width="160" />
      <el-table-column prop="deviceId" label="deviceId" width="160" />
      <el-table-column prop="stationKey" label="工站" width="120" />
      <el-table-column prop="action" label="动作" width="100">
        <template #default="{ row }">
          <el-tag :type="actionType(row.action)" size="small">{{ actionLabel(row.action) }}</el-tag>
        </template>
      </el-table-column>
      <el-table-column prop="ip" label="IP" width="140" />
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
import { ElMessage } from 'element-plus'
import { formatTime } from '../../utils/format'
import * as api from '../../api/audit'

const loading = ref(false)
const items = ref([])
const total = ref(0)
const page = ref(1)
const pageSize = 20
const filters = reactive({ username: '', hostName: '' })

function actionLabel(a) {
  const map = { login: '登录', logout: '退出', login_fail: '失败' }
  return map[a] || a
}

function actionType(a) {
  if (a === 'login') return 'success'
  if (a === 'login_fail') return 'danger'
  return 'info'
}

async function load() {
  loading.value = true
  try {
    const data = await api.listAuditLogins({
      page: page.value,
      pageSize,
      username: filters.username || undefined,
      hostName: filters.hostName || undefined,
    })
    items.value = data?.items || []
    total.value = data?.total || 0
  } catch (e) {
    items.value = []
    ElMessage.error(e.message)
  } finally {
    loading.value = false
  }
}

onMounted(load)
</script>

<style scoped>
.filter { margin-bottom: 16px; }
.pager { margin-top: 16px; justify-content: flex-end; }
</style>
