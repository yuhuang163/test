<template>
  <div>
    <el-form :inline="true" class="filter">
      <el-form-item label="电脑名">
        <el-input v-model="keyword" clearable placeholder="hostName" @keyup.enter="load" />
      </el-form-item>
      <el-form-item>
        <el-button type="primary" @click="load">查询</el-button>
        <el-button @click="openCreate">登记设备</el-button>
      </el-form-item>
    </el-form>

    <el-table :data="items" v-loading="loading" border>
      <el-table-column prop="hostName" label="电脑名" width="180" />
      <el-table-column prop="lineName" label="产线" width="120" />
      <el-table-column prop="stationLabel" label="工位" width="120" />
      <el-table-column prop="remark" label="备注" min-width="160" />
      <el-table-column prop="createdAt" label="登记时间" width="180">
        <template #default="{ row }">{{ formatTime(row.createdAt) }}</template>
      </el-table-column>
      <el-table-column label="操作" width="120" fixed="right">
        <template #default="{ row }">
          <el-button link type="primary" @click="openEdit(row)">编辑</el-button>
          <el-button link type="danger" @click="onDelete(row)">删除</el-button>
        </template>
      </el-table-column>
    </el-table>

    <el-empty v-if="!loading && !items.length" description="暂无登记设备">
      <el-button type="primary" @click="openCreate">登记第一台 PC</el-button>
    </el-empty>

    <el-dialog v-model="visible" :title="editId ? '编辑设备' : '登记设备'" width="480px" destroy-on-close>
      <el-form :model="form" label-width="90px">
        <el-form-item label="电脑名" required>
          <el-input v-model="form.hostName" :disabled="!!editId" placeholder="QSysInfo::machineHostName()" />
        </el-form-item>
        <el-form-item label="产线">
          <el-input v-model="form.lineName" />
        </el-form-item>
        <el-form-item label="工位">
          <el-input v-model="form.stationLabel" />
        </el-form-item>
        <el-form-item label="备注">
          <el-input v-model="form.remark" type="textarea" :rows="2" />
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
import { ElMessage, ElMessageBox } from 'element-plus'
import { formatTime } from '../../utils/format'
import * as api from '../../api/devices'

const loading = ref(false)
const submitting = ref(false)
const visible = ref(false)
const editId = ref(null)
const keyword = ref('')
const items = ref([])
const form = reactive({ hostName: '', lineName: '', stationLabel: '', remark: '' })

async function load() {
  loading.value = true
  try {
    const data = await api.listDevices({ keyword: keyword.value || undefined })
    items.value = data?.items || data || []
  } catch {
    items.value = []
  } finally {
    loading.value = false
  }
}

function openCreate() {
  editId.value = null
  Object.assign(form, { hostName: '', lineName: '', stationLabel: '', remark: '' })
  visible.value = true
}

function openEdit(row) {
  editId.value = row.id
  Object.assign(form, {
    hostName: row.hostName,
    lineName: row.lineName || '',
    stationLabel: row.stationLabel || '',
    remark: row.remark || '',
  })
  visible.value = true
}

async function onSubmit() {
  if (!form.hostName) {
    ElMessage.warning('请填写电脑名')
    return
  }
  submitting.value = true
  try {
    if (editId.value) {
      await api.updateDevice(editId.value, form)
    } else {
      await api.createDevice(form)
    }
    ElMessage.success('已保存')
    visible.value = false
    await load()
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    submitting.value = false
  }
}

async function onDelete(row) {
  try {
    await ElMessageBox.confirm(`确认删除设备 ${row.hostName}？`, '删除', { type: 'warning' })
    await api.deleteDevice(row.id)
    ElMessage.success('已删除')
    await load()
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  }
}

onMounted(load)
</script>

<style scoped>
.filter { margin-bottom: 16px; }
</style>
