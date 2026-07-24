<template>
  <pre class="log-preview" v-html="html"></pre>
</template>

<script setup>
import { computed } from 'vue'
import { highlightLogHtml } from '../utils/highlightLog'

const props = defineProps({
  text: {
    type: String,
    default: '',
  },
})

const html = computed(() => highlightLogHtml(props.text))
</script>

<style scoped>
.log-preview {
  margin: 0;
  max-height: var(--log-preview-max-height, 70vh);
  overflow: auto;
  white-space: pre-wrap;
  word-break: break-all;
  font-size: 12px;
  line-height: 1.5;
  background: #1e1e1e;
  color: #d4d4d4;
  padding: 12px;
  border-radius: 4px;
  font-family: Consolas, 'Courier New', monospace;
}

.log-preview :deep(.log-err),
.log-preview :deep(.log-fail) {
  color: #f44747;
  font-weight: 600;
}

.log-preview :deep(.log-wrn) {
  color: #f4ad42;
  font-weight: 600;
}

.log-preview :deep(.log-dbg) {
  color: #6a9955;
}

.log-preview :deep(.log-ui) {
  color: #569cd6;
  font-weight: 600;
}

.log-preview :deep(.log-time) {
  color: #858585;
}

.log-preview :deep(.log-pass) {
  color: #4ec9b0;
  font-weight: 600;
}
</style>
