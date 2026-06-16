<template>
  <div v-loading="loading">
    <el-form :model="form" label-width="100px" class="base-form">
      <el-form-item label="模板名" required>
        <el-input v-model="form.name" placeholder="如：自由工站默认阈值" style="max-width: 400px" />
      </el-form-item>
      <el-form-item label="工站" required>
        <el-select v-model="form.stationKey" placeholder="选择工站" style="width: 240px">
          <el-option v-for="s in meta.stations" :key="s.key" :label="`${s.name} (${s.key})`" :value="s.key" />
        </el-select>
      </el-form-item>
      <el-form-item label="产品型号">
        <el-input v-model="form.productModel" placeholder="可选，空表示通用" style="max-width: 240px" />
      </el-form-item>
    </el-form>

    <div class="section-head">
      <span>SETTINGS 键值</span>
      <el-button size="small" @click="addRow">添加一行</el-button>
    </div>
    <el-table :data="form.items" border size="small">
      <el-table-column label="settingsKey" min-width="220">
        <template #default="{ row }">
          <el-select v-model="row.settingsKey" filterable allow-create placeholder="如 BLE/LowRssi" style="width: 100%">
            <el-option v-for="k in meta.settingsKeys" :key="k" :label="k" :value="k" />
          </el-select>
        </template>
      </el-table-column>
      <el-table-column label="value" min-width="160">
        <template #default="{ row }">
          <el-input v-model="row.value" />
        </template>
      </el-table-column>
      <el-table-column label="操作" width="80">
        <template #default="{ $index }">
          <el-button link type="danger" @click="form.items.splice($index, 1)">删除</el-button>
        </template>
      </el-table-column>
    </el-table>

    <div class="actions">
      <el-button @click="router.back()">返回</el-button>
      <el-button type="primary" :loading="saving" @click="onSave">保存草稿</el-button>
      <el-button
        v-if="!isNew"
        type="success"
        :loading="publishing"
        @click="onPublish"
      >发布</el-button>
    </div>
  </div>
</template>

<script setup>
import { computed, onMounted, reactive, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { ElMessage, ElMessageBox } from 'element-plus'
import { useMetaStore } from '../../stores/meta'
import * as api from '../../api/thresholds'

const route = useRoute()
const router = useRouter()
const meta = useMetaStore()
const loading = ref(false)
const saving = ref(false)
const publishing = ref(false)

const isNew = computed(() => route.params.id === 'new')
const form = reactive({
  name: '',
  stationKey: '',
  productModel: '',
  items: [{ settingsKey: '', value: '' }],
})

function addRow() {
  form.items.push({ settingsKey: '', value: '' })
}

function buildBody() {
  const items = form.items.filter((x) => x.settingsKey?.trim())
  return {
    name: form.name,
    stationKey: form.stationKey,
    productModel: form.productModel || null,
    items,
  }
}

async function onSave() {
  if (!form.name || !form.stationKey) {
    ElMessage.warning('请填写模板名和工站')
    return
  }
  saving.value = true
  try {
    if (isNew.value) {
      const data = await api.createTemplate(buildBody())
      ElMessage.success('已保存')
      router.replace(`/config/thresholds/${data.id}`)
    } else {
      await api.updateTemplate(route.params.id, buildBody())
      ElMessage.success('已保存')
    }
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    saving.value = false
  }
}

async function onPublish() {
  try {
    await ElMessageBox.confirm('确认发布？上位机拉取后将获得新版本阈值。', '发布确认', { type: 'warning' })
    publishing.value = true
    await api.updateTemplate(route.params.id, buildBody())
    const data = await api.publishTemplate(route.params.id)
    ElMessage.success(`发布成功，版本 ${data?.version || ''}`)
    router.push('/config/thresholds')
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  } finally {
    publishing.value = false
  }
}

async function loadDetail() {
  if (isNew.value) return
  loading.value = true
  try {
    const data = await api.getTemplate(route.params.id)
    form.name = data.name || ''
    form.stationKey = data.stationKey || ''
    form.productModel = data.productModel || ''
    form.items = data.items?.length ? data.items : [{ settingsKey: '', value: '' }]
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    loading.value = false
  }
}

onMounted(async () => {
  await meta.load()
  await loadDetail()
})
</script>

<style scoped>
.base-form { max-width: 720px; margin-bottom: 20px; }
.section-head {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 8px;
  font-weight: 600;
}
.actions { margin-top: 24px; display: flex; gap: 8px; }
</style>
