<template>
  <div class="merge-diff-host">
    <div ref="hostRef" class="merge-diff-mount" />
  </div>
</template>

<script setup>
import { nextTick, onBeforeUnmount, onMounted, ref, watch } from 'vue'
import * as monaco from 'monaco-editor'
import editorWorkerUrl from 'monaco-editor/editor/editor.worker.js?url'

self.MonacoEnvironment = {
  getWorkerUrl() {
    return editorWorkerUrl
  },
}

const props = defineProps({
  original: { type: String, default: '' },
  modified: { type: String, default: '' },
  readOnlyModified: { type: Boolean, default: false },
})

const emit = defineEmits(['update:modified'])

const hostRef = ref(null)

let diffEditor = null
let originalModel = null
let modifiedModel = null
let applyingExternal = false
let contentDisposable = null
let resizeObserver = null
let layoutTimer = null

const sharedEditorOptions = {
  lineNumbersMinChars: 3,
  glyphMargin: false,
  folding: false,
  lineDecorationsWidth: 8,
  padding: { top: 0, bottom: 0 },
  scrollbar: {
    verticalScrollbarSize: 10,
    horizontalScrollbarSize: 10,
    alwaysConsumeMouseWheel: false,
  },
  overviewRulerLanes: 2,
  hideCursorInOverviewRuler: true,
  overviewRulerBorder: false,
}

function layoutSoon() {
  if (!diffEditor) return
  clearTimeout(layoutTimer)
  layoutTimer = setTimeout(() => {
    try {
      diffEditor?.layout()
    } catch (_) {
      /* ignore */
    }
  }, 30)
}

function ensureEditor() {
  if (diffEditor || !hostRef.value) return
  originalModel = monaco.editor.createModel(props.original ?? '', 'plaintext')
  modifiedModel = monaco.editor.createModel(props.modified ?? '', 'plaintext')
  diffEditor = monaco.editor.createDiffEditor(hostRef.value, {
    automaticLayout: false,
    readOnly: false,
    originalEditable: false,
    renderSideBySide: true,
    enableSplitViewResizing: true,
    renderIndicators: true,
    // 仅一侧有还原图标会拉开左右边距，关掉以保证文本列对齐
    renderMarginRevertIcon: false,
    // 概览尺只在右侧，会导致左右栏不等宽、标题对不齐
    renderOverviewRuler: false,
    ignoreTrimWhitespace: false,
    scrollBeyondLastLine: false,
    minimap: { enabled: false },
    fontSize: 13,
    lineHeight: 20,
    fontFamily: "Consolas, 'Courier New', monospace",
    lineNumbers: 'on',
    wordWrap: 'off',
    theme: 'vs',
    ...sharedEditorOptions,
  })
  diffEditor.setModel({ original: originalModel, modified: modifiedModel })

  const sameOpts = {
    ...sharedEditorOptions,
    readOnly: false,
  }
  diffEditor.getOriginalEditor().updateOptions({
    ...sameOpts,
    readOnly: true,
  })
  diffEditor.getModifiedEditor().updateOptions({
    ...sameOpts,
    readOnly: !!props.readOnlyModified,
  })

  contentDisposable = modifiedModel.onDidChangeContent(() => {
    if (applyingExternal) return
    emit('update:modified', modifiedModel.getValue())
  })

  // 弹窗 transform / 尺寸变化时强制重算，避免左右行错位
  resizeObserver = new ResizeObserver(() => layoutSoon())
  resizeObserver.observe(hostRef.value)
  nextTick(() => layoutSoon())
  // 等 Element Plus 弹窗动画结束再 layout 一次
  setTimeout(() => layoutSoon(), 280)
}

function syncModels() {
  if (!diffEditor || !originalModel || !modifiedModel) return
  applyingExternal = true
  try {
    if (originalModel.getValue() !== (props.original ?? '')) {
      originalModel.setValue(props.original ?? '')
    }
    if (modifiedModel.getValue() !== (props.modified ?? '')) {
      modifiedModel.setValue(props.modified ?? '')
    }
    diffEditor.getModifiedEditor().updateOptions({ readOnly: !!props.readOnlyModified })
    layoutSoon()
  } finally {
    applyingExternal = false
  }
}

onMounted(() => {
  ensureEditor()
  syncModels()
})

watch(
  () => [props.original, props.modified, props.readOnlyModified],
  () => syncModels()
)

onBeforeUnmount(() => {
  clearTimeout(layoutTimer)
  resizeObserver?.disconnect()
  resizeObserver = null
  contentDisposable?.dispose()
  contentDisposable = null
  diffEditor?.dispose()
  diffEditor = null
  originalModel?.dispose()
  modifiedModel?.dispose()
  originalModel = null
  modifiedModel = null
})
</script>

<style scoped>
.merge-diff-host,
.merge-diff-mount {
  width: 100%;
  height: 100%;
  min-height: 320px;
}
</style>
