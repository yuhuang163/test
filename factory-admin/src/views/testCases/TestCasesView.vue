<template>
  <div class="page">
    <div class="toolbar">
      <el-button :loading="downloading" @click="onDownload">下载 bundle</el-button>
      <el-button type="success" :loading="publishing" @click="onPublish">发布 bundle</el-button>
      <el-button :loading="saving" :disabled="!selectedPath" @click="onSave">保存当前文件</el-button>
      <el-button :disabled="!selectedPath" @click="onDelete">删除文件</el-button>
      <el-button @click="onNewFile">新建 ini</el-button>
      <el-button @click="loadTree">刷新</el-button>
      <span v-if="bundleVersion" class="ver">当前 bundle：{{ bundleVersion }}（{{ fileCount }} 个文件）</span>
    </div>
    <el-alert
      class="hint"
      type="info"
      :closable="false"
      show-icon
      title="测试用例由上位机在设置页「上传用例」打包上传；网页仅查看、在线编辑与发布。"
    />

    <div class="main">
      <el-card class="tree-panel" shadow="never">
        <template #header>test_case 文件</template>
        <el-tree
          v-loading="treeLoading"
          :data="treeData"
          node-key="path"
          highlight-current
          :props="{ label: 'label', children: 'children' }"
          @node-click="onSelect"
        />
        <el-empty v-if="!treeLoading && !treeData.length" description="暂无用例，请在上位机上传" />
      </el-card>

      <el-card class="editor-panel" shadow="never">
        <template #header>
          <span>{{ selectedPath || '请选择左侧文件' }}</span>
        </template>
        <el-input
          v-model="content"
          type="textarea"
          :rows="24"
          :disabled="!selectedPath"
          placeholder="ini 文本内容"
          class="editor"
          @input="dirty = true"
        />
      </el-card>
    </div>
  </div>
</template>

<script setup>
import { onMounted, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import * as api from '../../api/testCases'

const treeLoading = ref(false)
const saving = ref(false)
const publishing = ref(false)
const downloading = ref(false)
const treeData = ref([])
const selectedPath = ref('')
const content = ref('')
const bundleVersion = ref('')
const fileCount = ref(0)
const dirty = ref(false)

function toTree(files) {
  if (!files?.length) return []
  if (files[0]?.children !== undefined) return files
  return files.map((f) => ({
    label: f.path || f.name,
    path: f.path || f.name,
  }))
}

async function loadTree() {
  treeLoading.value = true
  try {
    const data = await api.listFiles()
    treeData.value = toTree(data?.files || data?.tree || data || [])
    bundleVersion.value = data?.bundleVersion || ''
    fileCount.value = data?.files?.length || treeData.value.length
  } catch {
    treeData.value = []
  } finally {
    treeLoading.value = false
  }
}

async function onSelect(node) {
  if (!node.path) return
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
  try {
    const { value } = await ElMessageBox.prompt('请输入文件名（如 示例步骤.ini）', '新建 ini', {
      confirmButtonText: '创建',
      inputPattern: /.+\.ini$/i,
      inputErrorMessage: '文件名须以 .ini 结尾',
    })
    const path = value.trim()
    await api.saveFile(path, '[Meta]\nName=新建用例\n')
    ElMessage.success('已创建')
    await loadTree()
    selectedPath.value = path
    content.value = '[Meta]\nName=新建用例\n'
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
    await ElMessageBox.confirm('确认发布测试用例 bundle？上位机将按新版本拉取。', '发布确认', { type: 'warning' })
    publishing.value = true
    const data = await api.publishBundle()
    bundleVersion.value = data?.bundleVersion || ''
    ElMessage.success(`发布成功，bundle ${bundleVersion.value}`)
    await loadTree()
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  } finally {
    publishing.value = false
  }
}

onMounted(loadTree)
</script>

<style scoped>
.page { height: calc(100vh - 120px); display: flex; flex-direction: column; }
.hint { margin-bottom: 12px; }
.toolbar { margin-bottom: 12px; display: flex; align-items: center; gap: 8px; flex-wrap: wrap; }
.ver { margin-left: auto; color: #666; font-size: 13px; }
.main { flex: 1; display: flex; gap: 12px; min-height: 0; }
.tree-panel { width: 280px; flex-shrink: 0; overflow: auto; }
.editor-panel { flex: 1; min-width: 0; }
.editor { font-family: Consolas, monospace; }
:deep(.el-textarea__inner) { font-family: Consolas, monospace; }
</style>
