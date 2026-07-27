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
        <el-select
          v-model="filters.station"
          clearable
          filterable
          placeholder="全部工站"
          style="width: 220px"
          :loading="stationLoading"
        >
          <el-option v-for="s in stations" :key="s" :label="s" :value="s" />
        </el-select>
      </el-form-item>
      <el-form-item label="日期段">
        <el-date-picker
          v-model="dateRange"
          type="daterange"
          range-separator="至"
          start-placeholder="开始日期"
          end-placeholder="结束日期"
          value-format="YYYY-MM-DD"
          :shortcuts="dateShortcuts"
          style="width: 260px"
        />
      </el-form-item>
      <el-form-item label="分组">
        <el-select v-model="groupBy" style="width: 100px">
          <el-option label="按天" value="day" />
          <el-option label="按周" value="week" />
          <el-option label="按月" value="month" />
        </el-select>
      </el-form-item>
      <el-form-item>
        <el-button type="primary" @click="loadYield">查询</el-button>
      </el-form-item>
    </el-form>

    <el-row :gutter="16" class="stat-row">
      <el-col :span="6">
        <el-card shadow="never" class="stat-card">
          <div class="stat-num primary">{{ stats.totalCount }}</div>
          <div class="stat-label">总测试数</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="never" class="stat-card">
          <div class="stat-num success">{{ stats.passCount }}</div>
          <div class="stat-label">通过数</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="never" class="stat-card">
          <div class="stat-num danger">{{ stats.failCount }}</div>
          <div class="stat-label">失败数</div>
        </el-card>
      </el-col>
      <el-col :span="6">
        <el-card shadow="never" class="stat-card">
          <div class="stat-num" :class="stats.overallPassRate >= 95 ? 'success' : stats.overallPassRate >= 80 ? 'warning' : 'danger'">
            {{ stats.overallPassRate }}%
          </div>
          <div class="stat-label">良率</div>
        </el-card>
      </el-col>
    </el-row>

    <el-row :gutter="16">
      <el-col :span="16">
        <el-card shadow="never" class="chart-card">
          <template #header>良率趋势</template>
          <div ref="trendChartRef" class="chart" v-loading="loading"></div>
          <div v-if="!stats.trend?.length && !loading" class="empty-hint">暂无数据</div>
        </el-card>
      </el-col>
      <el-col :span="8">
        <el-card shadow="never" class="chart-card">
          <template #header>良率分布</template>
          <div ref="pieChartRef" class="chart" v-loading="loading"></div>
          <div v-if="!stats.totalCount && !loading" class="empty-hint">暂无数据</div>
        </el-card>
      </el-col>
    </el-row>

    <el-card shadow="never" class="chart-card" v-if="stats.topFailItems?.length">
      <template #header>失败项 TOP10</template>
      <div ref="barChartRef" class="chart bar-chart"></div>
    </el-card>
  </div>
</template>

<script setup>
import { onMounted, reactive, ref, nextTick, watch } from 'vue'
import { ElMessage } from 'element-plus'
import { DATE_RANGE_SHORTCUTS, defaultDateRange, toApiTimeRange } from '../utils/dateRange'
import * as api from '../api/analytics'
import { useFactoryScope } from '../composables/useFactoryScope'
import http from '../api/http'
import * as echarts from 'echarts'

const { isFactoryScoped, scopedFactoryLabel, applyScopedFactoryFilter } = useFactoryScope()

const factories = ref([])
const stations = ref([])
const stationLoading = ref(false)
const loading = ref(false)
const filters = reactive({ factoryName: '', station: '' })
const dateRange = ref(defaultDateRange(7))
const dateShortcuts = DATE_RANGE_SHORTCUTS
const groupBy = ref('day')
const stats = reactive({ totalCount: 0, passCount: 0, failCount: 0, overallPassRate: 0, trend: [], topFailItems: [] })

const trendChartRef = ref(null)
const pieChartRef = ref(null)
const barChartRef = ref(null)
let trendChart = null
let pieChart = null
let barChart = null

async function loadFactories() {
  factories.value = await http.get('/admin/meta/factories')
}

async function loadStations() {
  stationLoading.value = true
  try {
    const data = await api.getAnalyticsStations({
      factoryName: filters.factoryName || undefined,
    })
    stations.value = data.stations || []
    if (filters.station && !stations.value.includes(filters.station)) {
      filters.station = ''
    }
  } catch {
    stations.value = []
  } finally {
    stationLoading.value = false
  }
}

async function loadYield() {
  loading.value = true
  try {
    const data = await api.getYieldStats({
      factoryName: filters.factoryName || undefined,
      station: filters.station || undefined,
      groupBy: groupBy.value,
      ...toApiTimeRange(dateRange.value),
    })
    Object.assign(stats, data)
    await nextTick()
    renderCharts()
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    loading.value = false
  }
}

function renderCharts() {
  renderTrend()
  renderPie()
  renderBar()
}

function renderTrend() {
  if (!trendChartRef.value) return
  if (!trendChart) trendChart = echarts.init(trendChartRef.value)
  const trend = stats.trend || []
  if (!trend.length) { trendChart.clear(); return }

  const periods = trend.map((t) => t.period)
  const passRates = trend.map((t) => t.passRate)

  trendChart.setOption({
    tooltip: {
      trigger: 'axis',
      formatter(params) {
        const idx = params[0]?.dataIndex
        const d = trend[idx]
        if (!d) return ''
        return `<div>${d.period}<br/>良率: ${d.passRate}%<br/>总测: ${d.total}&nbsp;&nbsp;通过: ${d.pass}&nbsp;&nbsp;失败: ${d.fail}</div>`
      },
    },
    grid: { left: 60, right: 30, top: 30, bottom: 40 },
    xAxis: {
      type: 'category', data: periods,
      axisLabel: { rotate: 30, fontSize: 11 },
      boundaryGap: false,
    },
    yAxis: {
      type: 'value',
      min: 0,
      max: 100,
      name: '良率 (%)',
      nameTextStyle: { fontSize: 12 },
      splitLine: { lineStyle: { type: 'dashed', color: '#e2e8f0' } },
    },
    series: [
      {
        name: '良率',
        type: 'line',
        data: passRates,
        smooth: true,
        symbol: 'circle',
        symbolSize: 6,
        lineStyle: { width: 2, color: '#10b981' },
        areaStyle: {
          color: new echarts.graphic.LinearGradient(0, 0, 0, 1, [
            { offset: 0, color: 'rgba(16,185,129,0.3)' },
            { offset: 1, color: 'rgba(16,185,129,0.02)' },
          ]),
        },
        itemStyle: { color: '#10b981' },
        markLine: {
          silent: true,
          data: [{ yAxis: 95, label: { formatter: '目标 95%', color: '#94a3b8' }, lineStyle: { color: '#f59e0b', type: 'dashed' } }],
        },
      },
    ],
  })
}

function renderPie() {
  if (!pieChartRef.value) return
  if (!pieChart) pieChart = echarts.init(pieChartRef.value)
  const total = stats.totalCount
  if (!total) { pieChart.clear(); return }

  pieChart.setOption({
    tooltip: { trigger: 'item', formatter: '{b}: {c} ({d}%)' },
    series: [
      {
        type: 'pie',
        radius: ['45%', '70%'],
        center: ['50%', '50%'],
        avoidLabelOverlap: true,
        label: { show: true, formatter: '{b}\n{d}%', fontSize: 12 },
        emphasis: { label: { show: true, fontWeight: 'bold' } },
        data: [
          { value: stats.passCount, name: '通过', itemStyle: { color: '#10b981' } },
          { value: stats.failCount, name: '失败', itemStyle: { color: '#ef4444' } },
        ],
      },
    ],
  })
}

function renderBar() {
  if (!barChartRef.value) return
  if (!barChart) barChart = echarts.init(barChartRef.value)
  const items = stats.topFailItems || []
  if (!items.length) { barChart.clear(); return }

  const names = items.map((i) => i.name).reverse()
  const counts = items.map((i) => i.failCount).reverse()

  barChart.setOption({
    tooltip: { trigger: 'axis', axisPointer: { type: 'shadow' } },
    grid: { left: 140, right: 30, top: 10, bottom: 20 },
    xAxis: { type: 'value', splitLine: { lineStyle: { type: 'dashed', color: '#e2e8f0' } } },
    yAxis: { type: 'category', data: names, axisLabel: { fontSize: 11 } },
    series: [
      {
        type: 'bar',
        data: counts.map((v) => ({ value: v, itemStyle: { color: new echarts.graphic.LinearGradient(0, 0, 1, 0, [
          { offset: 0, color: '#f97316' },
          { offset: 1, color: '#ef4444' },
        ]) } })),
        barMaxWidth: 24,
      },
    ],
  })
}

watch(() => filters.factoryName, () => {
  loadStations()
})

onMounted(async () => {
  await loadFactories()
  applyScopedFactoryFilter(filters)
  await loadStations()
  await loadYield()
})

window.addEventListener('resize', () => {
  trendChart?.resize()
  pieChart?.resize()
  barChart?.resize()
})
</script>

<style scoped>
.filter { margin-bottom: 16px; }
.stat-row { margin-bottom: 16px; }
.stat-card { text-align: center; padding: 8px 0; border-radius: 14px; border: none; }
.stat-num { font-size: 36px; font-weight: 700; line-height: 1.2; }
.stat-label { font-size: 14px; color: #94a3b8; margin-top: 6px; }
.primary { color: #3b82f6; }
.success { color: #10b981; }
.danger { color: #ef4444; }
.warning { color: #f59e0b; }
.chart-card { margin-bottom: 16px; border-radius: 14px; border: none; }
.chart { width: 100%; height: 360px; }
.bar-chart { height: 300px; }
.empty-hint { text-align: center; color: #94a3b8; padding: 60px 0; font-size: 14px; }
</style>
