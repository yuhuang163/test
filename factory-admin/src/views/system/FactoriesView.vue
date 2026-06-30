<template>
  <div>
    <div class="toolbar">
      <el-button type="primary" @click="openCreate">添加工厂</el-button>
      <el-button @click="load">刷新</el-button>
    </div>

    <el-alert
      class="hint"
      type="info"
      :closable="false"
      show-icon
      title="工厂保存到数据库后立即生效，无需重启 API。代码建议用小写英文（如 jj），批量账号将生成为 jjop / jjeng / jjadm。"
    />

    <el-table :data="items" v-loading="loading">
      <el-table-column prop="code" label="工厂代码" width="120" />
      <el-table-column prop="displayName" label="显示名" min-width="140" />
      <el-table-column label="状态" width="90">
        <template #default="{ row }">
          <el-tag :type="row.enabled ? 'success' : 'info'" size="small">
            {{ row.enabled ? '启用' : '停用' }}
          </el-tag>
        </template>
      </el-table-column>
      <el-table-column label="批量账号示例" min-width="200">
        <template #default="{ row }">{{ batchAccountHint(row.code) }}</template>
      </el-table-column>
      <el-table-column label="操作" width="100" fixed="right">
        <template #default="{ row }">
          <el-button link type="primary" @click="openEdit(row)">编辑</el-button>
        </template>
      </el-table-column>
    </el-table>

    <el-dialog v-model="visible" :title="editCode ? '编辑工厂' : '添加工厂'" width="480px" destroy-on-close>
      <el-form :model="form" label-width="96px">
        <el-form-item label="工厂代码" required>
          <el-input
            v-model="form.code"
            :disabled="!!editCode"
            placeholder="小写英文，如 jj（与上位机 Mes/FACTORY 一致）"
          />
        </el-form-item>
        <el-form-item label="显示名" required>
          <el-input v-model="form.displayName" placeholder="如：捷捷电子" />
        </el-form-item>
        <el-form-item v-if="editCode" label="状态">
          <el-switch v-model="form.enabled" active-text="启用" inactive-text="停用" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="visible = false">取消</el-button>
        <el-button type="primary" :loading="submitting" @click="onSubmit">保存</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { onMounted, reactive, ref } from 'vue'
import { ElMessage } from 'element-plus'
import { useMetaStore } from '../../stores/meta'
import * as api from '../../api/factories'

const meta = useMetaStore()
const loading = ref(false)
const submitting = ref(false)
const visible = ref(false)
const editCode = ref('')
const items = ref([])
const form = reactive({
  code: '',
  displayName: '',
  enabled: true,
})

function batchAccountHint(code) {
  const slug = (code || '').trim()
  if (!slug) return '-'
  return `${slug}op、${slug}eng、${slug}adm`
}

async function load() {
  loading.value = true
  try {
    items.value = (await api.listAllFactories()) || []
  } catch (e) {
    items.value = []
    ElMessage.error(e.message)
  } finally {
    loading.value = false
  }
}

function openCreate() {
  editCode.value = ''
  Object.assign(form, {
    code: '',
    displayName: '',
    enabled: true,
  })
  visible.value = true
}

function openEdit(row) {
  editCode.value = row.code
  Object.assign(form, {
    code: row.code,
    displayName: row.displayName,
    enabled: row.enabled !== false,
  })
  visible.value = true
}

async function onSubmit() {
  if (!form.code.trim() || !form.displayName.trim()) {
    ElMessage.warning('请填写工厂代码与显示名')
    return
  }
  submitting.value = true
  try {
    if (editCode.value) {
      await api.updateFactory(editCode.value, {
        displayName: form.displayName.trim(),
        enabled: form.enabled,
      })
    } else {
      await api.createFactory({
        code: form.code.trim(),
        displayName: form.displayName.trim(),
      })
    }
    ElMessage.success('已保存，无需重启服务')
    visible.value = false
    await load()
    await meta.reloadFactories()
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    submitting.value = false
  }
}

onMounted(load)
</script>

<style scoped>
.toolbar { margin-bottom: 16px; display: flex; gap: 8px; }
.hint { margin-bottom: 16px; }
</style>
