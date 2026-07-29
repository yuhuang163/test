<template>
  <div class="page">
    <div class="topbar">
      <div class="topbar-left">
        <h2 class="title">测试用例</h2>
        <el-tag v-if="bundleVersion" size="small" type="info" effect="plain">
          工作区 {{ bundleVersion }} · {{ stationCount }} 工站
        </el-tag>
      </div>
      <div class="topbar-right">
        <el-button @click="refreshAll">刷新</el-button>
        <router-link to="/config/test-cases/versions">
          <el-button plain>版本历史</el-button>
        </router-link>
        <el-button :loading="downloading" @click="onDownload">下载包</el-button>
        <el-button type="success" :loading="publishing" @click="onPublish">发布正式包</el-button>
      </div>
    </div>

    <el-tabs v-model="activeTab" class="main-tabs">
      <el-tab-pane name="edit">
        <template #label>
          <span>编辑工作区</span>
        </template>
        <div class="edit-layout">
          <aside class="side">
            <el-input
              v-model="stationSearch"
              clearable
              size="default"
              placeholder="搜索工站 / 用例"
              class="side-search"
            />
            <div class="side-actions">
              <el-button size="small" @click="onNewFile">新建用例</el-button>
            </div>
            <el-scrollbar class="side-scroll">
              <el-tree
                :key="stationSearch.trim() ? `search:${stationSearch}` : 'all'"
                v-loading="treeLoading"
                :data="filteredTreeData"
                node-key="id"
                highlight-current
                :default-expand-all="!!stationSearch.trim()"
                :props="{ label: 'label', children: 'children', isLeaf: 'isLeaf' }"
                @node-click="onSelect"
              >
                <template #default="{ data }">
                  <div class="tree-node" :class="{ 'is-station': data.isStation }">
                    <span class="tree-node-label" :title="data.label">{{ data.label }}</span>
                    <span
                      v-if="data.isStation && data.updatedAt"
                      class="tree-node-time"
                      :title="'最近更新 ' + formatTime(data.updatedAt)"
                    >
                      {{ formatTreeTime(data.updatedAt) }}
                    </span>
                  </div>
                </template>
              </el-tree>
              <el-empty
                v-if="!treeLoading && !filteredTreeData.length"
                :image-size="64"
                :description="stationSearch.trim() ? '无匹配结果' : '暂无工站'"
              />
            </el-scrollbar>
          </aside>

          <section class="editor">
            <div class="editor-bar">
              <div class="editor-meta">
                <div class="editor-path" :title="selectedPath">
                  {{ selectedPath || '从左侧选择工站下的用例进行编辑' }}
                </div>
                <div v-if="currentStationName" class="editor-updated">
                  文件夹「{{ currentStationName }}」最近更新：{{
                    currentStationUpdatedAtText || '—'
                  }}
                </div>
              </div>
              <div class="editor-actions">
                <el-button
                  type="primary"
                  :loading="saving"
                  :disabled="!selectedPath || !dirty"
                  @click="onSave"
                >
                  保存
                </el-button>
                <el-button :disabled="!selectedPath" @click="onDelete">删除</el-button>
              </div>
            </div>
            <el-input
              v-model="content"
              type="textarea"
              :disabled="!selectedPath"
              resize="none"
              placeholder="选择左侧用例后在此编辑 ini"
              class="editor-body"
              @input="dirty = true"
            />
          </section>
        </div>
      </el-tab-pane>

      <el-tab-pane name="staging">
        <template #label>
          <span>
            产线草稿
            <el-badge v-if="stagingItems.length" :value="stagingItems.length" class="tab-badge" />
          </span>
        </template>

        <div class="staging-toolbar">
          <el-select
            v-model="pullDeviceId"
            clearable
            filterable
            placeholder="选择在线产线机"
            style="width: 280px"
          >
            <el-option
              v-for="d in onlineDevices"
              :key="d.deviceId"
              :label="deviceLabel(d)"
              :value="d.deviceId"
            />
            <template #empty>
              <div class="select-empty">暂无在线设备<br />请确认上位机已登录并保持运行</div>
            </template>
          </el-select>
          <el-select
            v-model="pullStationKey"
            clearable
            filterable
            :disabled="!pullDeviceId"
            placeholder="选择产线工站名（可空=产线当前工站）"
            style="width: 260px"
          >
            <el-option
              v-for="s in pullDeviceStations"
              :key="s.stationKey || s.displayName"
              :label="s.displayName || s.stationKey"
              :value="s.stationKey || s.displayName"
            />
            <template #empty>
              <div class="select-empty">
                {{
                  pullDeviceId
                    ? '该产线暂无工站（需上位机心跳上报）'
                    : '请先选择在线产线机'
                }}
              </div>
            </template>
          </el-select>
          <el-button type="warning" :loading="pulling" :disabled="!pullDeviceId" @click="onPull">
            从产线拉取
          </el-button>
          <el-button @click="loadStaging">刷新草稿</el-button>
          <span class="staging-hint">上传/拉取后先看变更，再合入；最后到「编辑工作区」点发布</span>
        </div>

        <el-table
          v-loading="stagingLoading"
          :data="stagingItems"
          size="default"
          stripe
          empty-text="暂无产线草稿"
          class="staging-table"
        >
          <el-table-column prop="displayName" label="工站" min-width="160" show-overflow-tooltip />
          <el-table-column prop="hostName" label="来源电脑" min-width="120" show-overflow-tooltip />
          <el-table-column prop="profileVersion" label="版本" width="88" />
          <el-table-column label="来源" width="100">
            <template #default="{ row }">
              {{ row.source === 'pull' ? '网页拉取' : '主动上传' }}
            </template>
          </el-table-column>
          <el-table-column prop="remark" label="说明" min-width="200" show-overflow-tooltip />
          <el-table-column prop="uploadedAt" label="时间" min-width="160" show-overflow-tooltip />
          <el-table-column prop="fileCount" label="文件" width="72" />
          <el-table-column label="操作" width="180" fixed="right">
            <template #default="{ row }">
              <el-button type="primary" link @click="openMergePreview(row)">查看变更</el-button>
              <el-button type="danger" link @click="clearDraft(row)">清除</el-button>
            </template>
          </el-table-column>
        </el-table>
      </el-tab-pane>

      <el-tab-pane name="history" label="合入记录">
        <div class="staging-toolbar">
          <el-button @click="loadMergeHistory">刷新记录</el-button>
          <span class="staging-hint">撤销仅恢复工作区到合入前；已发布需重新发布才会影响产线</span>
        </div>
        <el-table
          v-loading="historyLoading"
          :data="mergeHistoryItems"
          size="default"
          stripe
          empty-text="暂无合入记录"
          class="staging-table"
        >
          <el-table-column prop="displayName" label="工站" min-width="140" show-overflow-tooltip />
          <el-table-column prop="hostName" label="来源电脑" min-width="110" show-overflow-tooltip />
          <el-table-column prop="remark" label="说明" min-width="180" show-overflow-tooltip />
          <el-table-column label="版本" width="120">
            <template #default="{ row }">
              {{ row.beforeProfileVersion || '—' }} → {{ row.afterProfileVersion || '—' }}
            </template>
          </el-table-column>
          <el-table-column prop="mergedBy" label="操作人" width="100" show-overflow-tooltip />
          <el-table-column prop="mergedAt" label="合入时间" min-width="160" show-overflow-tooltip />
          <el-table-column label="状态" width="100">
            <template #default="{ row }">
              <el-tag v-if="row.undone" size="small" type="info">已撤销</el-tag>
              <el-tag v-else size="small" type="success">有效</el-tag>
            </template>
          </el-table-column>
          <el-table-column label="操作" width="160" fixed="right">
            <template #default="{ row }">
              <el-button type="primary" link @click="openMergeHistoryDetail(row)">查看详情</el-button>
              <el-button
                type="warning"
                link
                :disabled="!row.canUndo"
                :loading="undoingId === row.mergeId"
                @click="undoMergeRecord(row)"
              >
                撤销
              </el-button>
            </template>
          </el-table-column>
        </el-table>
      </el-tab-pane>
    </el-tabs>

    <el-dialog
      v-model="mergeDialogVisible"
      :title="mergeDialogTitle"
      width="96%"
      top="3vh"
      destroy-on-close
      align-center
      class="merge-dialog"
      @closed="onMergeDialogClosed"
    >
      <div v-loading="mergeDiffLoading" class="merge-body">
        <div v-if="mergeDiff" class="merge-meta">
          <el-tag size="small" type="info">工作区 v{{ mergeDiff.currentProfileVersion || '—' }}</el-tag>
          <span>→</span>
          <el-tag size="small" type="warning">草稿 v{{ mergeDiff.stagingProfileVersion || '—' }}</el-tag>
          <el-tag size="small" type="success">+{{ mergeDiff.addedCount }}</el-tag>
          <el-tag size="small" type="danger">-{{ mergeDiff.removedCount }}</el-tag>
          <el-tag size="small" type="warning">~{{ mergeDiff.changedCount }}</el-tag>
          <span class="merge-tip">VS Code 风格对比：红删 / 绿增；右侧可直接改最终内容</span>
        </div>
        <div v-if="mergeDiff?.remark" class="merge-remark">说明：{{ mergeDiff.remark }}</div>

        <div v-if="mergeDiff && mergeAllFiles.length === 0" class="diff-hint">暂无文件</div>
        <div v-else-if="mergeDiff && mergeChangedFiles.length === 0" class="diff-hint">
          与工作区完全相同，无需合入。可清除草稿。
        </div>

        <div v-else-if="mergeDiff" class="merge-compare">
          <aside class="merge-file-list">
            <div
              v-for="f in mergeAllFiles"
              :key="f.path"
              class="merge-file-item"
              :class="{ active: selectedMergePath === f.path, edited: isFinalEdited(f.path) }"
              @click="selectMergeFile(f.path)"
            >
              <span class="file-status" :class="f.status">{{ statusLabel(f.status) }}</span>
              <span class="file-path">{{ f.path }}</span>
              <span v-if="isFinalEdited(f.path)" class="edited-dot">改</span>
            </div>
          </aside>

          <div class="merge-diff-wrap">
            <div class="merge-diff-toolbar">
              <el-button size="small" :disabled="!selectedMergePath" @click="useLeftAsFinal">
                采用左侧
              </el-button>
              <el-button size="small" :disabled="!selectedMergePath" @click="useStagingAsFinal">
                还原草稿
              </el-button>
            </div>
            <div class="diff-labels">
              <span class="diff-label left">工作区（当前）</span>
              <span class="diff-label right">最终内容（可改）</span>
            </div>
            <MergeDiffEditor
              v-if="selectedMergePath"
              :key="selectedMergePath"
              :original="selectedCurrentText"
              v-model:modified="selectedFinalText"
              class="merge-monaco"
            />
            <div v-else class="diff-hint">请选择左侧文件</div>
          </div>
        </div>
      </div>
      <template #footer>
        <el-button @click="mergeDialogVisible = false">取消</el-button>
        <el-button
          v-if="mergeDiff && mergeChangedFiles.length === 0"
          type="danger"
          :loading="clearing"
          @click="clearDraft(mergeTarget, true)"
        >
          清除草稿
        </el-button>
        <template v-else>
          <el-button type="danger" plain :loading="clearing" @click="clearDraft(mergeTarget, true)">
            清除草稿
          </el-button>
          <el-button type="primary" :loading="merging" :disabled="!mergeDiff" @click="confirmMerge">
            确认合入
          </el-button>
        </template>
      </template>
    </el-dialog>

    <el-dialog
      v-model="historyDialogVisible"
      :title="historyDialogTitle"
      width="96%"
      top="3vh"
      destroy-on-close
      align-center
      class="merge-dialog"
      @closed="onHistoryDialogClosed"
    >
      <div v-loading="historyDiffLoading" class="merge-body">
        <div v-if="historyDiff" class="merge-meta">
          <el-tag size="small" type="info">合入前 v{{ historyDiff.beforeProfileVersion || '—' }}</el-tag>
          <span>→</span>
          <el-tag size="small" type="warning">合入后 v{{ historyDiff.afterProfileVersion || '—' }}</el-tag>
          <el-tag size="small" type="success">+{{ historyDiff.addedCount }}</el-tag>
          <el-tag size="small" type="danger">-{{ historyDiff.removedCount }}</el-tag>
          <el-tag size="small" type="warning">~{{ historyDiff.changedCount }}</el-tag>
          <el-tag v-if="historyDiff.undone" size="small" type="info">已撤销</el-tag>
          <span class="merge-tip">只读对照合入前后快照</span>
        </div>
        <div v-if="historyDiff" class="merge-remark">
          <template v-if="historyDiff.remark">说明：{{ historyDiff.remark }}；</template>
          操作人：{{ historyDiff.mergedBy || '—' }}；
          时间：{{ historyDiff.mergedAt || '—' }}；
          来源：{{ historyDiff.hostName || historyDiff.deviceId || '—' }}
          <template v-if="historyDiff.undone">
            ；撤销人：{{ historyDiff.undoneBy || '—' }}（{{ historyDiff.undoneAt || '—' }}）
          </template>
        </div>

        <div v-if="historyDiff && historyAllFiles.length === 0" class="diff-hint">暂无文件</div>
        <div v-else-if="historyDiff && historyChangedFiles.length === 0" class="diff-hint">
          合入前后文件内容完全相同。
        </div>

        <div v-else-if="historyDiff" class="merge-compare">
          <aside class="merge-file-list">
            <div
              v-for="f in historyAllFiles"
              :key="f.path"
              class="merge-file-item"
              :class="{ active: selectedHistoryPath === f.path }"
              @click="selectedHistoryPath = f.path"
            >
              <span class="file-status" :class="f.status">{{ statusLabel(f.status) }}</span>
              <span class="file-path">{{ f.path }}</span>
            </div>
          </aside>

          <div class="merge-diff-wrap">
            <div class="diff-labels">
              <span class="diff-label left">合入前</span>
              <span class="diff-label right">合入后</span>
            </div>
            <MergeDiffEditor
              v-if="selectedHistoryPath"
              :key="`hist-${selectedHistoryPath}`"
              :original="selectedHistoryBeforeText"
              :modified="selectedHistoryAfterText"
              :read-only-modified="true"
              class="merge-monaco"
            />
            <div v-else class="diff-hint">请选择左侧文件</div>
          </div>
        </div>
      </div>
      <template #footer>
        <el-button type="primary" @click="historyDialogVisible = false">关闭</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { computed, defineAsyncComponent, onMounted, onUnmounted, ref, watch } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { formatTime, parseApiDateTime } from '../../utils/format'
import * as api from '../../api/testCases'

const MergeDiffEditor = defineAsyncComponent(() => import('../../components/MergeDiffEditor.vue'))

const activeTab = ref('edit')
const treeLoading = ref(false)
const stagingLoading = ref(false)
const historyLoading = ref(false)
const undoingId = ref('')
const saving = ref(false)
const publishing = ref(false)
const downloading = ref(false)
const pulling = ref(false)
const merging = ref(false)
const clearing = ref(false)
const treeData = ref([])
const stationSearch = ref('')
const selectedPath = ref('')
const content = ref('')
const bundleVersion = ref('')
const fileCount = ref(0)
const dirty = ref(false)
const stagingItems = ref([])
const mergeHistoryItems = ref([])
const onlineDevices = ref([])
const pullDeviceId = ref('')
const pullStationKey = ref('')
const currentStationName = ref('')
let onlineTimer = null

const stationCount = computed(() => treeData.value.length)

const currentStationUpdatedAt = computed(() => {
  if (!currentStationName.value) return ''
  const node = treeData.value.find((s) => s.stationName === currentStationName.value)
  return node?.updatedAt || ''
})

const currentStationUpdatedAtText = computed(() => formatTime(currentStationUpdatedAt.value))

/** 侧栏紧凑时间，给工站名留更多宽度 */
function formatTreeTime(v) {
  const d = parseApiDateTime(v)
  if (!d) return ''
  const parts = new Intl.DateTimeFormat('zh-CN', {
    timeZone: 'Asia/Shanghai',
    month: 'numeric',
    day: 'numeric',
    hour: '2-digit',
    minute: '2-digit',
    hour12: false,
  }).formatToParts(d)
  const get = (type) => parts.find((p) => p.type === type)?.value || ''
  return `${get('month')}/${get('day')} ${get('hour')}:${get('minute')}`
}

/** 当前选中产线机心跳上报的本机工站 */
const pullDeviceStations = computed(() => {
  const device = onlineDevices.value.find((d) => d.deviceId === pullDeviceId.value)
  const list = Array.isArray(device?.stations) ? device.stations : []
  return list
    .map((s) => ({
      stationKey: String(s?.stationKey || '').trim(),
      displayName: String(s?.displayName || s?.stationName || s?.stationKey || '').trim(),
    }))
    .filter((s) => s.stationKey || s.displayName)
})

watch(pullDeviceId, () => {
  // 切换产线机后清空，避免沿用上一台的工站键
  pullStationKey.value = ''
})

watch(pullDeviceStations, (list) => {
  if (!pullStationKey.value) return
  const ok = list.some(
    (s) => s.stationKey === pullStationKey.value || s.displayName === pullStationKey.value
  )
  if (!ok) pullStationKey.value = ''
})

const filteredTreeData = computed(() => {
  const kw = stationSearch.value.trim().toLowerCase()
  if (!kw) return treeData.value
  const result = []
  for (const station of treeData.value) {
    const stationHit =
      String(station.stationName || '').toLowerCase().includes(kw) ||
      String(station.label || '').toLowerCase().includes(kw)
    const matchedChildren = (station.children || []).filter((child) => {
      const label = String(child.label || '').toLowerCase()
      const path = String(child.path || '').toLowerCase()
      return label.includes(kw) || path.includes(kw)
    })
    if (stationHit) {
      result.push({ ...station, children: [...(station.children || [])] })
    } else if (matchedChildren.length) {
      result.push({
        ...station,
        label: `${station.stationName}（匹配 ${matchedChildren.length}）`,
        children: matchedChildren,
      })
    }
  }
  return result
})

const mergeDialogVisible = ref(false)
const mergeDiffLoading = ref(false)
const mergeDiff = ref(null)
const mergeTarget = ref(null)
const selectedMergePath = ref('')
/** 各文件合入后的最终文本（可编辑） */
const finalContents = ref({})

const historyDialogVisible = ref(false)
const historyDiffLoading = ref(false)
const historyDiff = ref(null)
const historyTarget = ref(null)
const selectedHistoryPath = ref('')

const mergeDialogTitle = computed(() => {
  const row = mergeTarget.value
  if (!row) return '合入预览'
  return `合入预览：${row.displayName || row.stationKey}`
})

const historyDialogTitle = computed(() => {
  const row = historyTarget.value || historyDiff.value
  if (!row) return '合入详情'
  return `合入详情：${row.displayName || row.stationKey || ''}`
})

const mergeAllFiles = computed(() => {
  if (!mergeDiff.value?.files) return []
  const order = { added: 0, changed: 1, removed: 2, unchanged: 3 }
  return [...mergeDiff.value.files].sort(
    (a, b) => (order[a.status] ?? 9) - (order[b.status] ?? 9) || a.path.localeCompare(b.path)
  )
})

const mergeChangedFiles = computed(() => mergeAllFiles.value.filter((f) => f.status !== 'unchanged'))

const historyAllFiles = computed(() => {
  if (!historyDiff.value?.files) return []
  const order = { added: 0, changed: 1, removed: 2, unchanged: 3 }
  return [...historyDiff.value.files].sort(
    (a, b) => (order[a.status] ?? 9) - (order[b.status] ?? 9) || a.path.localeCompare(b.path)
  )
})

const historyChangedFiles = computed(() =>
  historyAllFiles.value.filter((f) => f.status !== 'unchanged')
)

const selectedCurrentText = computed(() => {
  const path = selectedMergePath.value
  if (!path || !mergeDiff.value?.contents) return ''
  return mergeDiff.value.contents[path]?.current ?? ''
})

const selectedFinalText = computed({
  get() {
    const path = selectedMergePath.value
    if (!path) return ''
    return finalContents.value[path] ?? ''
  },
  set(val) {
    const path = selectedMergePath.value
    if (!path) return
    finalContents.value = { ...finalContents.value, [path]: val }
  },
})

const selectedHistoryBeforeText = computed(() => {
  const path = selectedHistoryPath.value
  if (!path || !historyDiff.value?.contents) return ''
  const c = historyDiff.value.contents[path]
  return c?.before ?? c?.current ?? ''
})

const selectedHistoryAfterText = computed(() => {
  const path = selectedHistoryPath.value
  if (!path || !historyDiff.value?.contents) return ''
  const c = historyDiff.value.contents[path]
  return c?.after ?? c?.staging ?? ''
})

function statusLabel(status) {
  return { added: '新增', removed: '删除', changed: '修改', unchanged: '相同' }[status] || status
}

function selectMergeFile(path) {
  selectedMergePath.value = path
}

function isFinalEdited(path) {
  const staging = mergeDiff.value?.contents?.[path]?.staging ?? ''
  const final = finalContents.value[path]
  if (final === undefined) return false
  return final !== staging
}

function useLeftAsFinal() {
  const path = selectedMergePath.value
  if (!path) return
  finalContents.value = {
    ...finalContents.value,
    [path]: mergeDiff.value?.contents?.[path]?.current ?? '',
  }
}

function useStagingAsFinal() {
  const path = selectedMergePath.value
  if (!path) return
  finalContents.value = {
    ...finalContents.value,
    [path]: mergeDiff.value?.contents?.[path]?.staging ?? '',
  }
}

function onMergeDialogClosed() {
  mergeDiff.value = null
  mergeTarget.value = null
  selectedMergePath.value = ''
  finalContents.value = {}
}

function onHistoryDialogClosed() {
  historyDiff.value = null
  historyTarget.value = null
  selectedHistoryPath.value = ''
}

function initFinalContentsFromDiff(diff) {
  const next = {}
  const contents = diff?.contents || {}
  for (const [path, c] of Object.entries(contents)) {
    // 默认最终内容 = 草稿；删除类文件草稿为空，表示合入后删除
    next[path] = c?.staging ?? ''
  }
  finalContents.value = next
  const firstChanged = (diff?.files || []).find((f) => f.status !== 'unchanged')
  selectedMergePath.value = firstChanged?.path || (diff?.files || [])[0]?.path || ''
}

function newerUpdatedAt(a, b) {
  const da = parseApiDateTime(a)
  const db = parseApiDateTime(b)
  if (!da) return b || ''
  if (!db) return a || ''
  return da.getTime() >= db.getTime() ? a : b
}

function toStationTree(files) {
  const stations = new Map()
  for (const f of files || []) {
    const full = String(f.path || f.name || '').replace(/\\/g, '/').replace(/^\/+/, '')
    if (!full) continue
    const parts = full.split('/')
    if (parts.length < 2 || parts[0] !== 'profiles') continue
    const stationName = parts[1]
    if (!stationName) continue
    if (!stations.has(stationName)) {
      stations.set(stationName, {
        id: `station:${stationName}`,
        label: stationName,
        stationName,
        isStation: true,
        updatedAt: '',
        children: [],
      })
    }
    const station = stations.get(stationName)
    // 工站文件夹最近更新 = 其下文件 mtime 最大值
    if (f.updatedAt) {
      station.updatedAt = newerUpdatedAt(station.updatedAt, f.updatedAt)
    }
    const relParts = parts.slice(2)
    if (!relParts.length) continue
    let label = relParts.join('/')
    let kind = 'file'
    if (relParts[0] === 'steps' && relParts.length === 2) {
      label = relParts[1].replace(/\.ini$/i, '')
      kind = 'case'
    } else if (relParts.length === 1 && /^profile\.ini$/i.test(relParts[0])) {
      label = '工站配置'
      kind = 'meta'
    } else if (relParts.length === 1 && /^flow\.ini$/i.test(relParts[0])) {
      label = '流程'
      kind = 'meta'
    }
    station.children.push({
      id: full,
      label,
      path: full,
      stationName,
      kind,
      updatedAt: f.updatedAt || '',
      isLeaf: true,
    })
  }

  const nodes = [...stations.values()].sort((a, b) => a.label.localeCompare(b.label, 'zh-CN'))
  for (const node of nodes) {
    node.children.sort((a, b) => {
      const order = { meta: 0, case: 1, file: 2 }
      const oa = order[a.kind] ?? 9
      const ob = order[b.kind] ?? 9
      if (oa !== ob) return oa - ob
      return a.label.localeCompare(b.label, 'zh-CN')
    })
    const caseCount = node.children.filter((c) => c.kind === 'case').length
    node.label = `${node.stationName}（${caseCount}）`
  }
  return nodes
}

function deviceLabel(d) {
  const station = d.stationName || d.stationKey || '未选工站'
  const age = typeof d.ageSec === 'number' ? `${d.ageSec}s前` : '在线'
  return `${d.hostName || d.deviceId} · ${station} · ${age}`
}

async function loadTree() {
  treeLoading.value = true
  try {
    const data = await api.listFiles()
    const files = data?.files || data?.tree || data || []
    treeData.value = toStationTree(files)
    bundleVersion.value = data?.bundleVersion || ''
    fileCount.value = Array.isArray(files) ? files.length : 0
  } catch {
    treeData.value = []
  } finally {
    treeLoading.value = false
  }
}

async function loadStaging() {
  stagingLoading.value = true
  try {
    const data = await api.listStaging()
    stagingItems.value = data?.items || []
  } catch (e) {
    stagingItems.value = []
    ElMessage.error(e.message)
  } finally {
    stagingLoading.value = false
  }
}

async function loadMergeHistory() {
  historyLoading.value = true
  try {
    const data = await api.listMergeHistory({ limit: 50 })
    mergeHistoryItems.value = data?.items || []
  } catch (e) {
    mergeHistoryItems.value = []
    ElMessage.error(e.message)
  } finally {
    historyLoading.value = false
  }
}

async function undoMergeRecord(row) {
  if (!row?.mergeId || !row.canUndo) return
  try {
    await ElMessageBox.confirm(
      `撤销「${row.displayName || row.stationKey}」这次合入？\n工作区将恢复为合入前内容（v${row.beforeProfileVersion || '—'}）。\n若已发布，需重新发布后产线才会更新。`,
      '撤销合入',
      { type: 'warning', confirmButtonText: '确认撤销' }
    )
  } catch {
    return
  }
  undoingId.value = row.mergeId
  try {
    await api.undoMerge(row.mergeId)
    ElMessage.success('已撤销合入，工作区已恢复')
    await Promise.all([loadTree(), loadMergeHistory()])
    activeTab.value = 'edit'
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    undoingId.value = ''
  }
}

async function openMergeHistoryDetail(row) {
  if (!row?.mergeId) return
  historyTarget.value = row
  historyDiff.value = null
  selectedHistoryPath.value = ''
  historyDialogVisible.value = true
  historyDiffLoading.value = true
  try {
    const diff = await api.mergeHistoryDiff(row.mergeId)
    historyDiff.value = diff
    const firstChanged = (diff?.files || []).find((f) => f.status !== 'unchanged')
    selectedHistoryPath.value = firstChanged?.path || (diff?.files || [])[0]?.path || ''
  } catch (e) {
    ElMessage.error(e.message)
    historyDialogVisible.value = false
  } finally {
    historyDiffLoading.value = false
  }
}

async function loadOnlineDevices() {
  try {
    const data = await api.listOnlineDevices()
    onlineDevices.value = data?.items || []
  } catch {
    onlineDevices.value = []
  }
}

async function refreshAll() {
  await Promise.all([loadTree(), loadStaging(), loadMergeHistory(), loadOnlineDevices()])
}

async function onSelect(node) {
  if (node.isStation || !node.path) {
    currentStationName.value = node.stationName || ''
    return
  }
  currentStationName.value = node.stationName || ''
  if (dirty.value && selectedPath.value) {
    try {
      await ElMessageBox.confirm('当前文件未保存，是否放弃修改？', '提示', { type: 'warning' })
    } catch {
      return
    }
  }
  selectedPath.value = node.path
  dirty.value = false
  try {
    const text = await api.getFile(node.path)
    content.value = typeof text === 'string' ? text : text?.content || ''
  } catch (e) {
    content.value = ''
    ElMessage.error(e.message)
  }
}

async function onSave() {
  if (!selectedPath.value) return
  saving.value = true
  try {
    await api.saveFile(selectedPath.value, content.value)
    dirty.value = false
    ElMessage.success('已保存')
    await loadTree()
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    saving.value = false
  }
}

async function onDelete() {
  if (!selectedPath.value) return
  try {
    await ElMessageBox.confirm(`确认删除「${selectedPath.value}」？`, '删除确认', { type: 'warning' })
    await api.deleteFile(selectedPath.value)
    selectedPath.value = ''
    content.value = ''
    dirty.value = false
    ElMessage.success('已删除')
    await loadTree()
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  }
}

async function onNewFile() {
  if (!currentStationName.value) {
    ElMessage.warning('请先在左侧点选一个工站')
    return
  }
  try {
    const { value } = await ElMessageBox.prompt(
      `在「${currentStationName.value}」下新建用例`,
      '新建用例',
      {
        confirmButtonText: '创建',
        inputPattern: /.+\.ini$/i,
        inputErrorMessage: '文件名须以 .ini 结尾',
        inputPlaceholder: '如 扫描连接蓝牙.ini',
      }
    )
    const fileName = value.trim()
    const path = `profiles/${currentStationName.value}/steps/${fileName}`
    await api.saveFile(path, `[Meta]\nName=${fileName.replace(/\.ini$/i, '')}\n`)
    ElMessage.success('已创建')
    await loadTree()
    selectedPath.value = path
    content.value = `[Meta]\nName=${fileName.replace(/\.ini$/i, '')}\n`
    dirty.value = false
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  }
}

async function onDownload() {
  downloading.value = true
  try {
    const blob = await api.downloadBundle()
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `test_case_${bundleVersion.value || 'bundle'}.zip`
    a.click()
    URL.revokeObjectURL(url)
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    downloading.value = false
  }
}

async function onPublish() {
  try {
    await ElMessageBox.confirm(
      '确认发布工作区？产线「下载工站用例」将按新版本拉取。',
      '发布确认',
      { type: 'warning' }
    )
    publishing.value = true
    const data = await api.publishBundle()
    bundleVersion.value = data?.bundleVersion || ''
    ElMessage.success(`发布成功：${bundleVersion.value}`)
    await loadTree()
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  } finally {
    publishing.value = false
  }
}

async function openMergePreview(row) {
  mergeTarget.value = row
  mergeDiff.value = null
  finalContents.value = {}
  selectedMergePath.value = ''
  mergeDialogVisible.value = true
  mergeDiffLoading.value = true
  try {
    const diff = await api.stagingDiff({
      deviceId: row.deviceId,
      stationKey: row.stationKey,
    })
    mergeDiff.value = diff
    initFinalContentsFromDiff(diff)
  } catch (e) {
    ElMessage.error(e.message)
    mergeDialogVisible.value = false
  } finally {
    mergeDiffLoading.value = false
  }
}

async function clearDraft(row, fromDialog = false) {
  if (!row?.deviceId || !row?.stationKey) return
  try {
    if (!fromDialog) {
      await ElMessageBox.confirm(
        `清除草稿「${row.displayName || row.stationKey}」？仅删除云端暂存，不影响工作区。`,
        '清除草稿',
        { type: 'warning' }
      )
    }
    clearing.value = true
    await api.clearStaging({
      deviceId: row.deviceId,
      stationKey: row.stationKey,
    })
    ElMessage.success('草稿已清除')
    if (fromDialog) mergeDialogVisible.value = false
    await loadStaging()
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  } finally {
    clearing.value = false
  }
}

async function confirmMerge() {
  const row = mergeTarget.value
  if (!row || !mergeDiff.value) return
  merging.value = true
  try {
    const fileOverrides = {}
    const deletePaths = []
    const contents = mergeDiff.value.contents || {}
    for (const path of Object.keys(contents)) {
      const final = finalContents.value[path] ?? contents[path]?.staging ?? ''
      if (final === '') {
        deletePaths.push(path)
      } else {
        fileOverrides[path] = final
      }
    }
    const data = await api.mergeStaging({
      deviceId: row.deviceId,
      stationKey: row.stationKey,
      fileOverrides,
      deletePaths,
    })
    ElMessage.success(`已合入 ${data?.mergedPath || row.displayName}，请切换到「编辑工作区」后发布`)
    mergeDialogVisible.value = false
    await Promise.all([loadTree(), loadStaging(), loadMergeHistory()])
    activeTab.value = 'edit'
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    merging.value = false
  }
}

async function onPull() {
  if (!pullDeviceId.value) return
  pulling.value = true
  const selected = pullDeviceStations.value.find(
    (s) => s.stationKey === pullStationKey.value || s.displayName === pullStationKey.value
  )
  const stationKey = (selected?.stationKey || pullStationKey.value || '').trim()
  const displayName = (selected?.displayName || stationKey).trim()
  try {
    await api.pullProfile({
      deviceId: pullDeviceId.value,
      stationKey,
      displayName,
    })
    ElMessage.success('已下发拉取命令，等待产线回传…')
    for (let i = 0; i < 8; i += 1) {
      await new Promise((r) => setTimeout(r, 3000))
      await loadStaging()
      const hit = stagingItems.value.find(
        (x) =>
          x.deviceId === pullDeviceId.value &&
          x.source === 'pull' &&
          (!stationKey ||
            x.stationKey === stationKey ||
            x.displayName === displayName)
      )
      if (hit) {
        ElMessage.success('已回传，可查看变更后合入')
        break
      }
    }
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    pulling.value = false
  }
}

onMounted(() => {
  refreshAll()
  onlineTimer = setInterval(loadOnlineDevices, 15000)
})

onUnmounted(() => {
  if (onlineTimer) clearInterval(onlineTimer)
})
</script>

<style scoped>
.page {
  height: calc(100vh - 120px);
  display: flex;
  flex-direction: column;
  gap: 8px;
  min-height: 0;
}

.topbar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  flex-wrap: wrap;
  flex-shrink: 0;
}
.topbar-left,
.topbar-right {
  display: flex;
  align-items: center;
  gap: 10px;
  flex-wrap: wrap;
}
.title {
  margin: 0;
  font-size: 18px;
  font-weight: 600;
  color: #1f1f1f;
}

.main-tabs {
  flex: 1;
  min-height: 0;
  display: flex;
  flex-direction: column;
}
.main-tabs :deep(.el-tabs__header) {
  margin-bottom: 8px;
}
.main-tabs :deep(.el-tabs__content) {
  flex: 1;
  min-height: 0;
}
.main-tabs :deep(.el-tab-pane) {
  height: 100%;
}
.tab-badge {
  margin-left: 6px;
  vertical-align: middle;
}
.tab-badge :deep(.el-badge__content) {
  transform: translateY(-2px);
}

.edit-layout {
  height: 100%;
  display: grid;
  grid-template-columns: 380px 1fr;
  gap: 12px;
  min-height: 0;
}

.side {
  display: flex;
  flex-direction: column;
  gap: 8px;
  min-height: 0;
  border: 1px solid #ebeef5;
  border-radius: 8px;
  background: #fff;
  padding: 10px;
}
.side-search { flex-shrink: 0; }
.side-actions { flex-shrink: 0; }
.side-scroll {
  flex: 1;
  min-height: 0;
}
.tree-node {
  display: flex;
  align-items: flex-start;
  justify-content: space-between;
  gap: 8px;
  flex: 1;
  min-width: 0;
  width: 0;
  padding-right: 4px;
  line-height: 1.35;
}
.tree-node-label {
  flex: 1;
  min-width: 0;
  white-space: normal;
  word-break: break-all;
  line-height: 1.35;
}
.tree-node-time {
  flex-shrink: 0;
  font-size: 11px;
  color: #909399;
  line-height: 1.35;
  white-space: nowrap;
  padding-top: 1px;
}
.side :deep(.el-tree-node__content) {
  height: auto;
  min-height: 26px;
  padding-top: 4px;
  padding-bottom: 4px;
  align-items: flex-start;
}
.side :deep(.el-tree-node__content > .tree-node) {
  flex: 1;
  min-width: 0;
}

.editor {
  display: flex;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
  border: 1px solid #ebeef5;
  border-radius: 8px;
  background: #fff;
  overflow: hidden;
}
.editor-bar {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 12px;
  padding: 10px 12px;
  border-bottom: 1px solid #ebeef5;
  background: #fafafa;
  flex-shrink: 0;
}
.editor-meta {
  min-width: 0;
  flex: 1;
}
.editor-path {
  font-size: 13px;
  color: #606266;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
  font-family: Consolas, 'Microsoft YaHei', monospace;
}
.editor-updated {
  margin-top: 4px;
  font-size: 12px;
  color: #909399;
}
.editor-actions {
  display: flex;
  gap: 8px;
  flex-shrink: 0;
}
.editor-body {
  flex: 1;
  min-height: 0;
  height: 100%;
}
.editor-body :deep(.el-textarea) {
  height: 100%;
}
.editor-body :deep(.el-textarea__inner) {
  height: 100% !important;
  border: none;
  border-radius: 0;
  box-shadow: none;
  font-family: Consolas, 'Microsoft YaHei', monospace;
  font-size: 13px;
  line-height: 1.55;
  padding: 12px 14px;
}

.staging-toolbar {
  display: flex;
  align-items: center;
  gap: 8px;
  flex-wrap: wrap;
  margin-bottom: 12px;
}
.staging-hint {
  color: #909399;
  font-size: 12px;
}
.select-empty {
  padding: 12px;
  text-align: center;
  color: #909399;
  font-size: 12px;
  line-height: 1.5;
}
.staging-table {
  width: 100%;
}

.merge-meta { display: flex; align-items: center; gap: 8px; flex-wrap: wrap; margin-bottom: 10px; }
.merge-remark {
  margin: -2px 0 12px;
  padding: 8px 10px;
  color: #606266;
  background: #f5f7fa;
  border-radius: 4px;
  line-height: 1.45;
  white-space: pre-wrap;
  word-break: break-word;
}
.merge-tip { color: #888; font-size: 12px; margin-left: 4px; }
.diff-hint { padding: 24px; text-align: center; color: #999; }
.merge-body { min-height: 420px; }
.merge-compare {
  display: grid;
  grid-template-columns: 220px 1fr;
  gap: 10px;
  height: calc(88vh - 160px);
  min-height: 420px;
}
.merge-file-list {
  border: 1px solid #e5e5e5;
  border-radius: 4px;
  overflow: auto;
  background: #fafafa;
}
.merge-file-item {
  display: flex;
  align-items: center;
  gap: 6px;
  padding: 8px 10px;
  cursor: pointer;
  font-size: 12px;
  border-bottom: 1px solid #f0f0f0;
}
.merge-file-item:hover { background: #f0f5ff; }
.merge-file-item.active { background: #e6f4ff; }
.merge-file-item.edited .file-path { color: #d48806; font-weight: 600; }
.edited-dot {
  font-size: 11px;
  color: #fff;
  background: #fa8c16;
  border-radius: 2px;
  padding: 0 4px;
  flex-shrink: 0;
}
.file-status {
  font-size: 11px; padding: 1px 6px; border-radius: 3px;
  flex-shrink: 0; font-weight: 600; min-width: 32px; text-align: center;
}
.file-status.added { background: #f6ffed; color: #52c41a; }
.file-status.removed { background: #fff1f0; color: #ff4d4f; }
.file-status.changed { background: #fff7e6; color: #fa8c16; }
.file-status.unchanged { background: #f5f5f5; color: #999; }
.file-path { font-family: Consolas, monospace; flex: 1; overflow: hidden; text-overflow: ellipsis; white-space: nowrap; }

.merge-diff-wrap {
  display: flex;
  flex-direction: column;
  min-width: 0;
  min-height: 0;
  height: 100%;
  border: 1px solid #e5e5e5;
  border-radius: 4px;
  overflow: hidden;
  background: #fff;
}
.merge-diff-toolbar {
  display: flex;
  align-items: center;
  justify-content: flex-end;
  gap: 6px;
  padding: 6px 10px;
  background: #fafafa;
  border-bottom: 1px solid #e8e8e8;
  flex-shrink: 0;
}
/* 与下方 Monaco 左右栏同宽 1:1，避免按钮挤占导致标题错位 */
.diff-labels {
  display: grid;
  grid-template-columns: 1fr 1fr;
  flex-shrink: 0;
  border-bottom: 1px solid #e8e8e8;
  background: #f7f7f7;
  font-size: 12px;
  font-weight: 600;
}
.diff-label {
  padding: 5px 12px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.diff-label.left {
  color: #666;
  border-right: 1px solid #e8e8e8;
}
.diff-label.right { color: #d48806; }
.merge-monaco {
  flex: 1;
  min-height: 0;
}

@media (max-width: 960px) {
  .edit-layout { grid-template-columns: 1fr; }
  .side { max-height: 280px; }
  .merge-compare { grid-template-columns: 1fr; height: auto; }
  .merge-file-list { max-height: 160px; }
  .merge-diff-wrap { height: 480px; }
}

</style>
