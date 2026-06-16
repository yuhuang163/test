<template>
  <div>
    <div class="toolbar">
      <el-button type="primary" @click="openCreate">新建账号</el-button>
      <el-button @click="load">刷新</el-button>
    </div>

    <el-table :data="items" v-loading="loading" border>
      <el-table-column prop="username" label="用户名" width="120" />
      <el-table-column label="角色" width="160">
        <template #default="{ row }">{{ (row.roles || []).join('、') }}</template>
      </el-table-column>
      <el-table-column label="工站" min-width="160">
        <template #default="{ row }">{{ (row.stationKeys || []).join('、') || '全部' }}</template>
      </el-table-column>
      <el-table-column prop="status" label="状态" width="90">
        <template #default="{ row }">
          <el-tag :type="row.status === 'active' ? 'success' : 'info'" size="small">
            {{ row.status === 'active' ? '启用' : '停用' }}
          </el-tag>
        </template>
      </el-table-column>
      <el-table-column prop="failedLoginCount" label="失败次数" width="90" />
      <el-table-column prop="lastLoginAt" label="最后登录" width="180">
        <template #default="{ row }">{{ formatTime(row.lastLoginAt) }}</template>
      </el-table-column>
      <el-table-column prop="lastLoginHost" label="最后电脑名" width="140" />
      <el-table-column label="操作" fixed="right" width="220">
        <template #default="{ row }">
          <el-button link type="primary" @click="openEdit(row)">编辑</el-button>
          <el-button link @click="onResetPwd(row)">重置密码</el-button>
          <el-button v-if="row.lockedUntil" link type="warning" @click="onUnlock(row)">解锁</el-button>
        </template>
      </el-table-column>
    </el-table>

    <el-dialog v-model="dialogVisible" :title="editId ? '编辑账号' : '新建账号'" width="520px" destroy-on-close>
      <el-form :model="form" label-width="100px">
        <el-form-item label="用户名" required>
          <el-input v-model="form.username" :disabled="!!editId" />
        </el-form-item>
        <el-form-item v-if="!editId" label="初始密码" required>
          <el-input v-model="form.password" show-password />
        </el-form-item>
        <el-form-item label="角色" required>
          <el-checkbox-group v-model="form.roles">
            <el-checkbox value="operator">operator</el-checkbox>
            <el-checkbox value="engineer">engineer</el-checkbox>
            <el-checkbox value="admin">admin</el-checkbox>
          </el-checkbox-group>
        </el-form-item>
        <el-form-item label="工站授权">
          <el-select v-model="form.stationKeys" multiple style="width: 100%">
            <el-option v-for="s in meta.stations" :key="s.key" :label="s.name" :value="s.key" />
          </el-select>
        </el-form-item>
        <el-form-item v-if="editId" label="状态">
          <el-radio-group v-model="form.status">
            <el-radio value="active">启用</el-radio>
            <el-radio value="disabled">停用</el-radio>
          </el-radio-group>
        </el-form-item>
        <el-form-item label="备注">
          <el-input v-model="form.remark" type="textarea" :rows="2" />
        </el-form-item>
      </el-form>
      <template #footer>
        <el-button @click="dialogVisible = false">取消</el-button>
        <el-button type="primary" :loading="submitting" @click="onSubmit">保存</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { onMounted, reactive, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { formatTime } from '../../utils/format'
import { useMetaStore } from '../../stores/meta'
import * as api from '../../api/users'

const meta = useMetaStore()
const loading = ref(false)
const submitting = ref(false)
const items = ref([])
const dialogVisible = ref(false)
const editId = ref(null)
const form = reactive({
  username: '',
  password: '',
  roles: ['operator'],
  stationKeys: [],
  status: 'active',
  remark: '',
})

async function load() {
  loading.value = true
  try {
    const data = await api.listUsers()
    items.value = data?.items || data || []
  } catch {
    items.value = []
  } finally {
    loading.value = false
  }
}

function openCreate() {
  editId.value = null
  Object.assign(form, {
    username: '',
    password: '',
    roles: ['operator'],
    stationKeys: [],
    status: 'active',
    remark: '',
  })
  dialogVisible.value = true
}

function openEdit(row) {
  editId.value = row.id
  Object.assign(form, {
    username: row.username,
    password: '',
    roles: [...(row.roles || [])],
    stationKeys: [...(row.stationKeys || [])],
    status: row.status || 'active',
    remark: row.remark || '',
  })
  dialogVisible.value = true
}

async function onSubmit() {
  if (!form.username || (!editId.value && !form.password)) {
    ElMessage.warning('请填写必填项')
    return
  }
  submitting.value = true
  try {
    if (editId.value) {
      await api.updateUser(editId.value, {
        roles: form.roles,
        stationKeys: form.stationKeys,
        status: form.status,
        remark: form.remark,
      })
    } else {
      await api.createUser({
        username: form.username,
        password: form.password,
        roles: form.roles,
        stationKeys: form.stationKeys,
        remark: form.remark,
      })
    }
    ElMessage.success('已保存')
    dialogVisible.value = false
    await load()
  } catch (e) {
    ElMessage.error(e.message)
  } finally {
    submitting.value = false
  }
}

async function onResetPwd(row) {
  try {
    await ElMessageBox.confirm(`确认重置用户 ${row.username} 的密码？`, '重置密码', { type: 'warning' })
    const data = await api.resetPassword(row.id)
    await ElMessageBox.alert(`新密码：${data?.password || data?.newPassword || '（见后端返回）'}`, '请复制保存', {
      confirmButtonText: '已复制',
    })
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  }
}

async function onUnlock(row) {
  try {
    await api.unlockUser(row.id)
    ElMessage.success('已解锁')
    await load()
  } catch (e) {
    ElMessage.error(e.message)
  }
}

onMounted(async () => {
  await meta.load()
  await load()
})
</script>

<style scoped>
.toolbar { margin-bottom: 16px; display: flex; gap: 8px; }
</style>
