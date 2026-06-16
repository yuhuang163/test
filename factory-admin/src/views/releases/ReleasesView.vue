<template>
  <div>
    <div class="toolbar">
      <el-button type="primary" @click="openDialog">新建发布单</el-button>
      <el-button @click="load">刷新</el-button>
    </div>

    <el-table :data="items" v-loading="loading" border>
      <el-table-column prop="releaseId" label="发布单号" width="140" />
      <el-table-column prop="note" label="说明" min-width="160" />
      <el-table-column label="包含产物" min-width="220">
        <template #default="{ row }">
          <el-tag v-if="row.thresholdVersion" size="small" class="tag">阈值 v{{ row.thresholdVersion }}</el-tag>
          <el-tag v-if="row.bundleVersion" size="small" type="success" class="tag">用例 {{ row.bundleVersion }}</el-tag>
          <el-tag v-if="row.hostBuildId" size="small" type="warning" class="tag">exe {{ row.hostBuildId }}</el-tag>
        </template>
      </el-table-column>
      <el-table-column prop="createdBy" label="操作人" width="100" />
      <el-table-column prop="createdAt" label="时间" width="180">
        <template #default="{ row }">{{ formatTime(row.createdAt) }}</template>
      </el-table-column>
    </el-table>

    <el-empty v-if="!loading && !items.length" description="暂无统一发布记录">
      <el-button type="primary" @click="openDialog">创建发布单</el-button>
    </el-empty>

    <el-dialog v-model="visible" title="新建统一发布" width="560px" destroy-on-close>
      <el-form :model="form" label-width="120px">
        <el-form-item label="发布单号" required>
          <el-input v-model="form.releaseId" placeholder="如 2026-06-16-01" />
        </el-form-item>
        <el-form-item label="说明">
          <el-input v-model="form.note" type="textarea" :rows="2" />
        </el-form-item>
        <el-form-item label="阈值版本">
          <el-input v-model="form.thresholdVersion" placeholder="可选，已发布的 threshold version" />
        </el-form-item>
        <el-form-item label="用例 bundle">
          <el-input v-model="form.bundleVersion" placeholder="可选" />
        </el-form-item>
        <el-form-item label="上位机 buildId">
          <el-input v-model="form.hostBuildId" placeholder="可选" />
        </el-form-item>
        <el-form-item label="灰度工站">
          <el-select v-model="form.grayStationKeys" multiple style="width: 100%">
            <el-option v-for="s in meta.stations" :key="s.key" :label="s.name" :value="s.key" />
          </el-select>
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="visible = false">取消</el-button>
        <el-button type="primary" :loading="submitting" @click="onSubmit">发布</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { onMounted, reactive, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { formatTime } from '../../utils/format'
import { useMetaStore } from '../../stores/meta'
import * as api from '../../api/releases'

const meta = useMetaStore()
const loading = ref(false)
const submitting = ref(false)
const visible = ref(false)
const items = ref([])
const form = reactive({
  releaseId: '',
  note: '',
  thresholdVersion: '',
  bundleVersion: '',
  hostBuildId: '',
  grayStationKeys: [],
})

function openDialog() {
  Object.assign(form, {
    releaseId: '',
    note: '',
    thresholdVersion: '',
    bundleVersion: '',
    hostBuildId: '',
    grayStationKeys: [],
  })
  visible.value = true
}

async function load() {
  loading.value = true
  try {
    const data = await api.listReleases()
    items.value = data?.items || data || []
  } catch {
    items.value = []
  } finally {
    loading.value = false
  }
}

async function onSubmit() {
  if (!form.releaseId) {
    ElMessage.warning('请填写发布单号')
    return
  }
  try {
    await ElMessageBox.confirm(`确认创建发布单 ${form.releaseId}？`, '发布确认', { type: 'warning' })
    submitting.value = true
    await api.createRelease({
      releaseId: form.releaseId,
      note: form.note,
      thresholdVersion: form.thresholdVersion || null,
      bundleVersion: form.bundleVersion || null,
      hostBuildId: form.hostBuildId || null,
      grayRules: { stationKeys: form.grayStationKeys },
    })
    ElMessage.success('发布单已创建')
    visible.value = false
    await load()
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  } finally {
    submitting.value = false
  }
}

onMounted(async () => {
  await meta.load()
  await load()
})
</script>

<style scoped>
.toolbar { margin-bottom: 16px; display: flex; gap: 8px; }
.tag { margin-right: 4px; margin-bottom: 4px; }
</style>
