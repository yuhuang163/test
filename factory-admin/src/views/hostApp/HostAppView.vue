<template>
  <div>
    <el-tabs v-model="activeTab">
      <el-tab-pane label="版本管理" name="versions">
        <div class="toolbar">
          <el-button type="primary" @click="openDialog()">新建版本</el-button>
          <el-button @click="load">刷新</el-button>
        </div>

        <el-table :data="items" v-loading="loading">
          <el-table-column prop="appVersion" label="appVersion" width="100" />
          <el-table-column prop="buildId" label="buildId" width="120" />
          <el-table-column prop="packageName" label="包名" min-width="120" />
          <el-table-column prop="size" label="大小" width="90">
            <template #default="{ row }">{{ formatSize(row.size) }}</template>
          </el-table-column>
          <el-table-column prop="forceUpgrade" label="强制升级" width="90">
            <template #default="{ row }">
              <el-tag :type="row.forceUpgrade ? 'danger' : 'info'" size="small">
                {{ row.forceUpgrade ? '是' : '否' }}
              </el-tag>
            </template>
          </el-table-column>
          <el-table-column label="灰度" min-width="140">
            <template #default="{ row }">{{ grayText(row.grayRules) }}</template>
          </el-table-column>
          <el-table-column label="说明" min-width="160">
            <template #default="{ row }">{{ row.releaseNotes || '-' }}</template>
          </el-table-column>
          <el-table-column prop="uploadedAt" label="上传时间" width="170">
            <template #default="{ row }">{{ row.uploadedAt || '-' }}</template>
          </el-table-column>
          <el-table-column label="操作" width="80" fixed="right">
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
              <el-upload :auto-upload="false" :limit="1" :on-change="onFileChange" accept=".exe">
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
      </el-tab-pane>

      <el-tab-pane label="运行环境" name="runtime">
        <el-alert type="info" :closable="false" show-icon class="env-hint">
          <template #title>
            首次使用上位机时，需下载「路特上位机运行环境」。该环境包含 Qt 运行库、驱动和上位机主程序，自带升级功能，后续只需单独上传 exe 即可升级。
          </template>
        </el-alert>

        <div class="env-upload-bar">
          <span class="env-label">上传新版本：</span>
          <el-upload :auto-upload="false" :limit="1" :on-change="onEnvFileChange" accept=".zip">
            <el-button>选择 zip</el-button>
          </el-upload>
          <el-button type="success" :loading="envUploading" :disabled="!envFile" @click="onUploadEnv">
            上传
          </el-button>
        </div>

        <div v-if="envInfo.exists" class="env-card">
          <el-descriptions :column="2" border>
            <el-descriptions-item label="包含文件">{{ envInfo.fileCount }} 个</el-descriptions-item>
            <el-descriptions-item label="总大小">{{ formatSize(envInfo.sizeBytes) }}</el-descriptions-item>
            <el-descriptions-item label="内置 exe buildId">{{ envInfo.buildId || '-' }}</el-descriptions-item>
          </el-descriptions>
          <div class="env-actions">
            <el-button type="primary" size="large" :loading="envDownloading" @click="onDownloadEnv">
              下载运行环境
            </el-button>
          </div>
        </div>
        <el-empty v-else description="服务器上未上传运行环境" />
      </el-tab-pane>
    </el-tabs>
  </div>
</template>

<script setup>
import { onMounted, reactive, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { useMetaStore } from '../../stores/meta'
import * as api from '../../api/hostApp'

const meta = useMetaStore()
const activeTab = ref('versions')
const loading = ref(false)
const submitting = ref(false)
const items = ref([])
const dialogVisible = ref(false)
const editing = ref(false)
const exeFile = ref(null)
const envInfo = ref({})
const envDownloading = ref(false)
const envUploading = ref(false)
const envFile = ref(null)

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

function formatSize(bytes) {
  if (!bytes) return '-'
  if (bytes < 1024) return bytes + ' B'
  if (bytes < 1024 * 1024) return (bytes / 1024).toFixed(1) + ' KB'
  return (bytes / (1024 * 1024)).toFixed(1) + ' MB'
}

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
    form.appVersion = row.appVersion || ''
    form.buildId = row.buildId || ''
    form.packageName = row.packageName || 'new_production'
    form.releaseNotes = row.releaseNotes || ''
    form.forceUpgrade = !!row.forceUpgrade
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
    const label = `${form.appVersion} (buildId ${form.buildId})`
    await ElMessageBox.confirm(`确认发布版本 ${label}？`, '发布确认', { type: 'warning' })
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

async function loadEnvInfo() {
  try {
    const data = await api.getRuntimeEnvInfo()
    envInfo.value = data?.data || data || {}
  } catch {
    envInfo.value = {}
  }
}

function onEnvFileChange(uploadFile) {
  envFile.value = uploadFile.raw
}

async function onUploadEnv() {
  if (!envFile.value) return
  envUploading.value = true
  try {
    const fd = new FormData()
    fd.append('file', envFile.value)
    await api.uploadRuntimeEnv(fd)
    ElMessage.success('运行环境已上传')
    envFile.value = null
    await loadEnvInfo()
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    envUploading.value = false
  }
}

async function onDownloadEnv() {
  envDownloading.value = true
  try {
    const blob = await api.downloadRuntimeEnv()
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = '路特上位机运行环境.zip'
    a.click()
    URL.revokeObjectURL(url)
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    envDownloading.value = false
  }
}

onMounted(async () => {
  await meta.load()
  await load()
  await loadEnvInfo()
})
</script>

<style scoped>
.toolbar { margin-bottom: 16px; display: flex; gap: 8px; }
.hash { font-size: 12px; color: #666; margin-top: 8px; word-break: break-all; }
.env-hint { margin-bottom: 20px; }
.env-upload-bar { display: flex; align-items: center; gap: 12px; margin-bottom: 20px; }
.env-label { font-size: 14px; font-weight: 600; white-space: nowrap; }
.env-card { max-width: 600px; }
.env-actions { margin-top: 20px; }
</style>
