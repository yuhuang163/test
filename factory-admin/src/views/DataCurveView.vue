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
      <el-form-item label="测试项">
        <el-select
          v-model="filters.itemName"
          clearable
          filterable
          remote
          reserve-keyword
          placeholder="输入测试项名称"
          style="width: 220px"
          :remote-method="searchItems"
          :loading="nameLoading"
        >
          <el-option v-for="n in itemNames" :key="n" :label="n" :value="n" />
        </el-select>
      </el-form-item>
      <el-form-item label="最近">
        <el-select v-model="limit" style="width: 100px">
          <el-option label="50条" :value="50" />
          <el-option label="100条" :value="100" />
          <el-option label="200条" :value="200" />
          <el-option label="500条" :value="500" />
        </el-select>
      </el-form-item>
      <el-form-item>
        <el-button type="primary" @click="loadCurve">查询</el-button>
      </el-form-item>
    </el-form>

    <el-card shadow="never" class="chart-card">
      <template #header>
        <span>数据曲线 {{ filters.itemName ? '- ' + filters.itemName : '' }}</span>
        <span v-if="currentUnit" class="unit">({{ currentUnit }})</span>
      </template>
      <div ref="chartRef" class="chart" v-loading="loading"></div>
      <div v-if="!points.length && !loading" class="empty-hint">请选择测试项后查询</div>
    </el-card>

    <el-card shadow="never" class="detail-card" v-if="points.length">
      <template #header>
        <span>数据明细（{{ points.length }} 条）</span>
      </template>
      <el-table :data="points" border size="small" max-height="360">
        <el-table-column label="测试时间" width="170">
          <template #default="{ row }">{{ formatTime(row.testedAt) }}</template>
        </el-table-column>
        <el-table-column prop="sn" label="SN" width="140" />
        <el-table-column prop="value" label="实测值" width="100" />
        <el-table-column prop="maxValue" label="上限" width="80" />
        <el-table-column prop="minValue" label="下限" width="80" />
        <el-table-column prop="standardValue" label="标准值" width="80" />
        <el-table-column prop="unit" label="单位" width="60" />
        <el-table-column prop="result" label="单项结果" width="90" />
        <el-table-column prop="testResult" label="整机结果" width="90" />
      </el-table>
    </el-card>
  </div>
</template>

<script setup>
import { onMounted, reactive, ref, watch, nextTick } from 'vue'
import { ElMessage } from 'element-plus'
import { formatTime } from '../utils/format'
import * as api from '../api/analytics'
import http from '../api/http'
import * as echarts from 'echarts'

const factories = ref([])
const itemNames = ref([])
const nameLoading = ref(false)
const loading = ref(false)
const points = ref([])
const chartRef = ref(null)
let chartInstance = null

const filters = reactive({ factoryName: '', station: '', itemName: '' })
const limit = ref(200)

const currentUnit = ref('')

async function loadFactories() {
  factories.value = await http.get('/admin/meta/factories')
}

async function searchItems(keyword) {
  if (!keyword && !filters.factoryName && !filters.station) return
  nameLoading.value = true
  try {
    const data = await api.getCurveItemNames({
      factoryName: filters.factoryName || undefined,
      station: filters.station || undefined,
      keyword: keyword || undefined,
    })
    itemNames.value = data.names || []
  } catch {
    itemNames.value = []
  } finally {
    nameLoading.value = false
  }
}

async function loadCurve() {
  if (!filters.itemName) {
    ElMessage.warning('请选择测试项')
    return
  }
  loading.value = true
  try {
    const data = await api.getCurveData({
      factoryName: filters.factoryName || undefined,
      station: filters.station || undefined,
      itemName: filters.itemName,
      limit: limit.value,
    })
    points.value = data.points || []
    currentUnit.value = points.value[0]?.unit || ''
    await nextTick()
    renderChart()
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    loading.value = false
  }
}

function renderChart() {
  if (!chartRef.value) return
  if (!chartInstance) {
    chartInstance = echarts.init(chartRef.value)
  }
  const pts = points.value
  if (!pts.length) {
    chartInstance.clear()
    return
  }

  const categories = pts.map((p) => p.sn || p.testedAt || '')
  const values = pts.map((p) => (p.value ? parseFloat(p.value) : null))
  const maxVals = pts.map((p) => (p.maxValue ? parseFloat(p.maxValue) : null))
  const minVals = pts.map((p) => (p.minValue ? parseFloat(p.minValue) : null))
  const stdVals = pts.map((p) => (p.standardValue ? parseFloat(p.standardValue) : null))

  const series = [
    {
      name: '实测值',
      type: 'line',
      data: values,
      connectNulls: false,
      symbol: 'circle',
      symbolSize: 6,
      lineStyle: { width: 2 },
      itemStyle: { color: '#3b82f6' },
    },
  ]

  const hasMax = maxVals.some((v) => v !== null)
  const hasMin = minVals.some((v) => v !== null)
  const hasStd = stdVals.some((v) => v !== null)

  if (hasMax) {
    series.push({
      name: '上限',
      type: 'line',
      data: maxVals,
      connectNulls: false,
      symbol: 'none',
      lineStyle: { type: 'dashed', width: 1.5, color: '#ef4444' },
      itemStyle: { color: '#ef4444' },
    })
  }
  if (hasMin) {
    series.push({
      name: '下限',
      type: 'line',
      data: minVals,
      connectNulls: false,
      symbol: 'none',
      lineStyle: { type: 'dashed', width: 1.5, color: '#ef4444' },
      itemStyle: { color: '#ef4444' },
    })
  }
  if (hasStd) {
    series.push({
      name: '标准值',
      type: 'line',
      data: stdVals,
      connectNulls: false,
      symbol: 'none',
      lineStyle: { type: 'dotted', width: 1.5, color: '#10b981' },
      itemStyle: { color: '#10b981' },
    })
  }

  chartInstance.setOption({
    tooltip: {
      trigger: 'axis',
      formatter(params) {
        const idx = params[0]?.dataIndex
        const p = pts[idx]
        if (!p) return ''
        const lines = params.map((s) => `${s.marker} ${s.seriesName}: ${s.value ?? '-'}`)
        return `<div><strong>${p.sn || ''}</strong><br/>${lines.join('<br/>')}<br/>时间: ${formatTime(p.testedAt) || '-'}</div>`
      },
    },
    legend: { top: 0 },
    grid: { left: 60, right: 30, top: 40, bottom: 50 },
    xAxis: {
      type: 'category',
      data: categories,
      axisLabel: { rotate: 45, fontSize: 11, interval: Math.max(0, Math.floor(categories.length / 30) - 1) },
      boundaryGap: false,
    },
    yAxis: {
      type: 'value',
      name: currentUnit.value,
      nameTextStyle: { fontSize: 12 },
      splitLine: { lineStyle: { type: 'dashed', color: '#e2e8f0' } },
    },
    dataZoom: [
      { type: 'inside', start: 0, end: 100 },
      { type: 'slider', bottom: 10, height: 20, start: 0, end: 100 },
    ],
    series,
  })
}

watch(() => filters.factoryName, () => { searchItems('') })
watch(() => filters.station, () => { searchItems('') })

onMounted(async () => {
  await loadFactories()
})

window.addEventListener('resize', () => {
  chartInstance?.resize()
})
</script>

<style scoped>
.filter { margin-bottom: 16px; }
.chart-card { margin-bottom: 16px; }
.chart { width: 100%; height: 420px; }
.unit { color: #94a3b8; font-size: 13px; margin-left: 6px; font-weight: 400; }
.empty-hint { text-align: center; color: #94a3b8; padding: 60px 0; font-size: 14px; }
.detail-card :deep(.el-card__header) { font-weight: 600; }
</style>
