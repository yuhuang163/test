<template>
  <el-tabs v-model="activeTab" type="card">
    <el-tab-pane label="数据曲线" name="curve">
      <DataCurveView />
    </el-tab-pane>
    <el-tab-pane label="良率统计" name="yield">
      <YieldView />
    </el-tab-pane>
  </el-tabs>
</template>

<script setup>
import { ref } from 'vue'
import DataCurveView from './DataCurveView.vue'
import YieldView from './YieldView.vue'

const activeTab = ref('curve')
</script>

<style scoped>
:deep(.el-tabs__header) { margin-bottom: 16px; }
:deep(.el-tabs__item) { font-size: 14px; font-weight: 500; }
</style>
