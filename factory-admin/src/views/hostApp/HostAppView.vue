<template>
  <div>
    <div class="toolbar">
      <el-button type="primary" @click="openDialog()">新建版本</el-button>
      <el-button @click="load">刷新</el-button>
    </div>

    <el-table :data="items" v-loading="loading" border>
      <el-table-column prop="appVersion" label="appVersion" width="100" />
      <el-table-column prop="buildId" label="buildId" width="120" />
      <el-table-column prop="packageName" label="包名" min-width="140" />
      <el-table-column prop="forceUpgrade" label="强制升级" width="90">
        <template #default="{ row }">
          <el-tag :type="row.forceUpgrade ? 'danger' : 'info'" size="small">
            {{ row.forceUpgrade ? '是' : '否' }}
          </el-tag>
        </template>
      </el-table-column>
      <el-table-column label="灰度" min-width="160">
        <template #default="{ row }">{{ grayText(row.grayRules) }}</template>
      </el-table-column>
      <el-table-column prop="publishedAt" label="发布时间" width="180">
        <template #default="{ row }">{{ formatTime(row.publishedAt) }}</template>
      </el-table-column>
      <el-table-column label="操作" width="100" fixed="right">
        <template #default="{ row }">
          <el-button link type="primary" @click="openDialog(row)">详情</el-button>
        </template>
      </el-table-column>
    </el-table>

    <el-empty v-if="!loading && !items.length" description="暂无上位机版本">
      <el-button type="primary" @click="openDialog()">上传第一个版本</el-button>
    </el-empty>

    <el-dialog v-model="dialogVisible" :title="editing ? '版本详情' : '新建上位机版本'" width="640px" destroy-on-close>
      <el-form :model="form" label-width="110px">
        <el-form-item label="exe 文件" v-if="!editing">
          <el-upload :auto-upload="false" :limit="1" :on-change="onFileChange">
            <el-button>选择 exe</el-button>
          </el-upload>
          <div v-if="form.sha256" class="hash">sha256: {{ form.sha256 }}</div>
        </el-form-item>
        <el-form-item label="appVersion" required>
          <el-input v-model="form.appVersion" :disabled="editing" placeholder="如 1.1.15" />
        </el-form-item>
        <el-form-item label="buildId" required>
          <el-input v-model="form.buildId" :disabled="editing" placeholder="如 20260616" />
        </el-form-item>
        <el-form-item label="packageName">
          <el-input v-model="form.packageName" :disabled="editing" placeholder="默认 new_production" />
        </el-form-item>
        <el-form-item label="releaseNotes">
          <el-input v-model="form.releaseNotes" type="textarea" :rows="3" :disabled="editing" />
        </el-form-item>
        <el-form-item label="强制升级">
          <el-switch v-model="form.forceUpgrade" :disabled="editing" />
        </el-form-item>
        <el-form-item label="灰度工站">
          <el-select v-model="form.grayStationKeys" multiple :disabled="editing" style="width: 100%">
            <el-option v-for="s in meta.stations" :key="s.key" :label="s.name" :value="s.key" />
          </el-select>
        </el-form-item>
        <el-form-item label="灰度工厂">
          <el-select v-model="form.grayFactories" multiple :disabled="editing" style="width: 100%">
            <el-option v-for="f in meta.factories" :key="f.code" :label="f.displayName" :value="f.code" />
          </el-select>
        </el-form-item>
        <el-form-item label="deviceId 白名单">
          <el-input
            v-model="form.grayDeviceIdsText"
            type="textarea"
            :rows="2"
            :disabled="editing"
            placeholder="每行一个，空表示不限制"
          />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="dialogVisible = false">关闭</el-button>
        <el-button v-if="!editing" type="primary" :loading="submitting" @click="onSubmit">提交</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { onMounted, reactive, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { formatTime } from '../../utils/format'
import { useMetaStore } from '../../stores/meta'
import * as api from '../../api/hostApp'

const meta = useMetaStore()
const loading = ref(false)
const submitting = ref(false)
const items = ref([])
const dialogVisible = ref(false)
const editing = ref(false)
const exeFile = ref(null)

const form = reactive({
  appVersion: '',
  buildId: '',
  packageName: 'new_production',
  releaseNotes: '',
  forceUpgrade: false,
  sha256: '',
  grayStationKeys: [],
  grayFactories: [],
  grayDeviceIdsText: '',
})

function grayText(rules) {
  if (!rules) return '-'
  const parts = []
  if (rules.stationKeys?.length) parts.push(`工站:${rules.stationKeys.join(',')}`)
  if (rules.factories?.length) parts.push(`工厂:${rules.factories.join(',')}`)
  if (rules.deviceIds?.length) parts.push(`设备:${rules.deviceIds.length}台`)
  return parts.join('；') || '全量'
}

function resetForm() {
  Object.assign(form, {
    appVersion: '',
    buildId: '',
    packageName: 'new_production',
    releaseNotes: '',
    forceUpgrade: false,
    sha256: '',
    grayStationKeys: [],
    grayFactories: [],
    grayDeviceIdsText: '',
  })
  exeFile.value = null
}

function openDialog(row) {
  resetForm()
  editing.value = !!row
  if (row) {
    form.appVersion = row.appVersion
    form.buildId = row.buildId
    form.packageName = row.packageName
    form.releaseNotes = row.releaseNotes
    form.forceUpgrade = row.forceUpgrade
    form.grayStationKeys = row.grayRules?.stationKeys || []
    form.grayFactories = row.grayRules?.factories || []
    form.grayDeviceIdsText = (row.grayRules?.deviceIds || []).join('\n')
  }
  dialogVisible.value = true
}

async function sha256File(file) {
  const buf = await file.arrayBuffer()
  const hash = await crypto.subtle.digest('SHA-256', buf)
  return Array.from(new Uint8Array(hash))
    .map((b) => b.toString(16).padStart(2, '0'))
    .join('')
}

async function onFileChange(uploadFile) {
  exeFile.value = uploadFile.raw
  form.sha256 = await sha256File(uploadFile.raw)
}

async function load() {
  loading.value = true
  try {
    const data = await api.listVersions()
    items.value = data?.items || data || []
  } catch {
    items.value = []
  } finally {
    loading.value = false
  }
}

async function onSubmit() {
  if (!form.appVersion || !form.buildId) {
    ElMessage.warning('请填写 appVersion 和 buildId')
    return
  }
  try {
    await ElMessageBox.confirm(
      `确认发布版本 ${form.appVersion} (buildId ${form.buildId})？`,
      '发布确认',
      { type: 'warning' }
    )
    submitting.value = true
    const grayRules = {
      stationKeys: form.grayStationKeys,
      factories: form.grayFactories,
      deviceIds: form.grayDeviceIdsText.split('\n').map((s) => s.trim()).filter(Boolean),
    }
    if (exeFile.value) {
      const fd = new FormData()
      fd.append('file', exeFile.value)
      fd.append('appVersion', form.appVersion)
      fd.append('buildId', form.buildId)
      fd.append('packageName', form.packageName)
      fd.append('releaseNotes', form.releaseNotes)
      fd.append('forceUpgrade', String(form.forceUpgrade))
      fd.append('sha256', form.sha256)
      fd.append('grayRules', JSON.stringify(grayRules))
      await api.uploadVersion(fd)
    } else {
      await api.createVersion({
        appVersion: form.appVersion,
        buildId: form.buildId,
        packageName: form.packageName,
        releaseNotes: form.releaseNotes,
        forceUpgrade: form.forceUpgrade,
        sha256: form.sha256,
        grayRules,
      })
    }
    ElMessage.success('版本已提交')
    dialogVisible.value = false
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
.hash { font-size: 12px; color: #666; margin-top: 8px; word-break: break-all; }
</style>
