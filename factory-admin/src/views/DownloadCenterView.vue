<template>
  <div class="download-page" v-loading="loading">
    <div class="page-intro">
      <h2>上位机资源下载</h2>
      <p>产线换机、新机部署或手工更新时，按顺序下载下方三件资源即可。已在用的产线机也可直接在上位机「帮助 → 检查更新」拉取最新 EXE，在「帮助 → 更新测试用例」同步用例。</p>
    </div>

    <el-alert
      class="guide"
      type="info"
      :closable="false"
      show-icon
      title="推荐步骤：① 下载「运行环境」解压到电脑 → ② 下载「最新 EXE」覆盖解压目录中的主程序（或产线机打开「帮助 → 检查更新」在线升级）→ ③ 下载「测试用例」：产线机用「帮助 → 更新测试用例」，或手工解压覆盖 test_case 目录。"
    />

    <el-row :gutter="16" class="cards">
      <el-col :xs="24" :md="8">
        <el-card shadow="never" class="dl-card">
          <div class="card-badge">1</div>
          <h3>运行环境</h3>
          <p class="desc">首次使用必备。含 Qt 运行库、驱动与基础文件，解压后即可运行上位机。</p>
          <div class="meta" v-if="summary.runtimeEnv?.exists">
            <div><span>文件数</span><b>{{ summary.runtimeEnv.fileCount ?? '-' }}</b></div>
            <div><span>包大小</span><b>{{ formatSize(summary.runtimeEnv.sizeBytes) }}</b></div>
            <div v-if="summary.runtimeEnv.buildId"><span>内含 build</span><b>{{ summary.runtimeEnv.buildId }}</b></div>
          </div>
          <el-empty v-else description="暂未上传运行环境" :image-size="48" />
          <el-button
            type="primary"
            class="dl-btn"
            :disabled="!summary.runtimeEnv?.exists"
            :loading="busy.runtime"
            @click="onDownloadRuntime"
          >
            下载运行环境
          </el-button>
        </el-card>
      </el-col>

      <el-col :xs="24" :md="8">
        <el-card shadow="never" class="dl-card">
          <div class="card-badge">2</div>
          <h3>最新上位机 EXE</h3>
          <p class="desc">云端最新主程序。新机可下载覆盖使用；产线机也可在上位机「帮助 → 检查更新」直接在线升级。</p>
          <div class="meta" v-if="summary.hostApp">
            <div><span>版本</span><b>{{ summary.hostApp.appVersion || '-' }}</b></div>
            <div><span>buildId</span><b>{{ summary.hostApp.buildId || '-' }}</b></div>
            <div><span>大小</span><b>{{ formatSize(summary.hostApp.size) }}</b></div>
            <div><span>上传时间</span><b>{{ summary.hostApp.uploadedAt || '-' }}</b></div>
            <div v-if="summary.hostApp.releaseNotes" class="notes">
              <span>说明</span>
              <b>{{ summary.hostApp.releaseNotes }}</b>
            </div>
          </div>
          <el-empty v-else description="暂无上传的上位机版本" :image-size="48" />
          <el-button
            type="primary"
            class="dl-btn"
            :disabled="!summary.hostApp"
            :loading="busy.exe"
            @click="onDownloadExe"
          >
            下载最新 EXE
          </el-button>
        </el-card>
      </el-col>

      <el-col :xs="24" :md="8">
        <el-card shadow="never" class="dl-card">
          <div class="card-badge">3</div>
          <h3>测试用例</h3>
          <p class="desc">当前已发布的 test_case 包。产线机可在上位机「帮助 → 更新测试用例」拉取，也可从此处手工下载。</p>
          <div class="meta" v-if="summary.testCases?.bundleVersion || summary.testCases?.fileCount">
            <div><span>用例包版本</span><b>{{ summary.testCases.bundleVersion || '-' }}</b></div>
            <div><span>文件数</span><b>{{ summary.testCases.fileCount ?? '-' }}</b></div>
            <div v-if="summary.testCases.updatedAt"><span>发布时间</span><b>{{ summary.testCases.updatedAt }}</b></div>
          </div>
          <el-empty v-else description="暂无测试用例包" :image-size="48" />
          <el-button
            type="primary"
            class="dl-btn"
            :disabled="!(summary.testCases?.fileCount > 0 || summary.testCases?.bundleVersion)"
            :loading="busy.cases"
            @click="onDownloadCases"
          >
            下载测试用例
          </el-button>
        </el-card>
      </el-col>
    </el-row>

    <div class="footer-actions">
      <el-button @click="load">刷新信息</el-button>
    </div>
  </div>
</template>

<script setup>
import { onMounted, reactive, ref } from 'vue'
import { ElMessage } from 'element-plus'
import * as api from '../api/downloads'

const loading = ref(false)
const summary = ref({
  hostApp: null,
  testCases: {},
  runtimeEnv: {},
})
const busy = reactive({
  runtime: false,
  exe: false,
  cases: false,
})

function formatSize(n) {
  const num = Number(n)
  if (!Number.isFinite(num) || num <= 0) return '-'
  const units = ['B', 'KB', 'MB', 'GB']
  let v = num
  for (let i = 0; i < units.length; i++) {
    if (v < 1024 || i === units.length - 1) {
      return i === 0 ? `${Math.round(v)} ${units[i]}` : `${v.toFixed(2)} ${units[i]}`
    }
    v /= 1024
  }
  return `${num} B`
}

function saveBlob(blob, filename) {
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = filename
  a.click()
  URL.revokeObjectURL(url)
}

async function load() {
  loading.value = true
  try {
    const data = await api.getDownloadSummary({ packageName: 'new_production' })
    summary.value = {
      hostApp: data?.hostApp || null,
      testCases: data?.testCases || {},
      runtimeEnv: data?.runtimeEnv || {},
    }
  } catch (e) {
    summary.value = { hostApp: null, testCases: {}, runtimeEnv: {} }
    ElMessage.error(e.message || '加载失败')
  } finally {
    loading.value = false
  }
}

async function onDownloadRuntime() {
  busy.runtime = true
  try {
    const blob = await api.downloadRuntimeEnv()
    saveBlob(blob, '路特上位机运行环境.zip')
    ElMessage.success('运行环境已开始下载')
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    busy.runtime = false
  }
}

async function onDownloadExe() {
  busy.exe = true
  try {
    const host = summary.value.hostApp
    const blob = await api.downloadHostExe({ packageName: host?.packageName || 'new_production' })
    const name =
      host?.fileName ||
      `${host?.packageName || 'new_production'}_${host?.buildId || 'latest'}.exe`
    saveBlob(blob, name)
    ElMessage.success('上位机 EXE 已开始下载')
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    busy.exe = false
  }
}

async function onDownloadCases() {
  busy.cases = true
  try {
    const blob = await api.downloadTestCases()
    const ver = summary.value.testCases?.bundleVersion || 'bundle'
    saveBlob(blob, `test_case_${ver}.zip`)
    ElMessage.success('测试用例已开始下载')
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    busy.cases = false
  }
}

onMounted(load)
</script>

<style scoped>
.download-page {
  max-width: 1100px;
}
.page-intro h2 {
  margin: 0 0 8px;
  font-size: 20px;
  color: var(--admin-text);
}
.page-intro p {
  margin: 0 0 16px;
  color: var(--admin-text-secondary);
  font-size: 14px;
  line-height: 1.6;
}
.guide {
  margin-bottom: 20px;
}
.cards {
  margin-bottom: 8px;
}
.dl-card {
  height: 100%;
  margin-bottom: 16px;
  border-radius: var(--admin-radius-lg);
  position: relative;
  min-height: 320px;
  display: flex;
  flex-direction: column;
}
.dl-card :deep(.el-card__body) {
  display: flex;
  flex-direction: column;
  height: 100%;
  box-sizing: border-box;
}
.card-badge {
  width: 28px;
  height: 28px;
  border-radius: 50%;
  background: var(--admin-primary-light);
  color: var(--admin-primary);
  font-weight: 700;
  display: flex;
  align-items: center;
  justify-content: center;
  margin-bottom: 10px;
}
.dl-card h3 {
  margin: 0 0 8px;
  font-size: 16px;
  color: var(--admin-text);
}
.desc {
  margin: 0 0 14px;
  font-size: 13px;
  color: var(--admin-text-secondary);
  line-height: 1.55;
  min-height: 60px;
}
.meta {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-bottom: 16px;
  flex: 1;
}
.meta > div {
  display: flex;
  justify-content: space-between;
  gap: 12px;
  font-size: 13px;
}
.meta span {
  color: var(--admin-text-tertiary);
  flex-shrink: 0;
}
.meta b {
  color: var(--admin-text);
  text-align: right;
  word-break: break-all;
  font-weight: 600;
}
.meta .notes b {
  font-weight: 400;
  color: var(--admin-text-secondary);
}
.dl-btn {
  width: 100%;
  margin-top: auto;
}
.footer-actions {
  margin-top: 8px;
}
</style>
