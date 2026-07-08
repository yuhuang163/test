<template>
  <div>
    <div class="toolbar">
      <el-button type="primary" @click="openCreate">新建账号</el-button>
      <el-button type="success" plain @click="openBatchImport">批量导入</el-button>
      <el-button @click="copyAllPasswords">复制账号密码</el-button>
      <el-button @click="load">刷新</el-button>
    </div>

    <el-table :data="items" v-loading="loading">
      <el-table-column prop="username" label="用户名" width="110" />
      <el-table-column label="密码" width="130">
        <template #default="{ row }">
          <span v-if="row.password" class="pwd-text">{{ row.password }}</span>
          <span v-else class="pwd-empty">—</span>
        </template>
      </el-table-column>
      <el-table-column label="角色" width="200">
        <template #default="{ row }">{{ formatRoleLabels(row.roles) }}</template>
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
      <el-table-column label="操作" fixed="right" width="280">
        <template #default="{ row }">
          <el-button link type="primary" @click="openEdit(row)">编辑</el-button>
          <el-button link @click="onResetPwd(row)">重置密码</el-button>
          <el-button v-if="row.lockedUntil" link type="warning" @click="onUnlock(row)">解锁</el-button>
          <el-button link type="danger" @click="onDelete(row)">删除</el-button>
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
          <el-checkbox-group v-model="form.roles" class="role-group">
            <el-tooltip
              v-for="opt in ROLE_OPTIONS"
              :key="opt.value"
              :content="opt.tip"
              placement="top"
            >
              <span class="role-item">
                <el-checkbox :value="opt.value">{{ opt.label }}</el-checkbox>
              </span>
            </el-tooltip>
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

    <el-dialog v-model="batchVisible" title="批量导入账号" width="860px" destroy-on-close @opened="refreshBatchPreview">
      <el-alert
        class="batch-hint"
        type="info"
        :closable="false"
        show-icon
        title="每个工厂生成 3 个账号：产线操作员 / 工艺工程师 / 管理员。用户名如 lxop、lxeng、lxadm；密码为「用户名+26」，如 lxop26。"
      />
      <el-form label-width="88px" class="batch-form">
        <el-form-item label="工厂">
          <el-select
            v-model="batchForm.factoryCodes"
            multiple
            collapse-tags
            collapse-tags-tooltip
            placeholder="不选则包含全部启用工厂"
            style="width: 100%"
            @change="refreshBatchPreview"
          >
            <el-option
              v-for="f in meta.factories"
              :key="f.code"
              :label="`${f.displayName}（${f.code}）`"
              :value="f.code"
            />
          </el-select>
        </el-form-item>
        <el-form-item label="角色">
          <el-checkbox-group v-model="batchForm.roles" @change="refreshBatchPreview">
            <el-checkbox v-for="opt in ROLE_OPTIONS" :key="opt.value" :value="opt.value">
              {{ opt.label }}
            </el-checkbox>
          </el-checkbox-group>
        </el-form-item>
        <el-form-item label="工站授权">
          <el-select
            v-model="batchForm.stationKeys"
            multiple
            style="width: 100%"
            placeholder="不选表示不限制工站"
          >
            <el-option v-for="s in meta.stations" :key="s.key" :label="s.name" :value="s.key" />
          </el-select>
        </el-form-item>
        <el-form-item label="已存在">
          <el-checkbox v-model="batchForm.skipExisting">跳过已有用户名（不覆盖）</el-checkbox>
        </el-form-item>
      </el-form>

      <div class="batch-toolbar">
        <span v-if="batchSummary.total" class="batch-summary">
          共 {{ batchSummary.total }} 个，待创建 {{ batchSummary.toCreate }}，已存在 {{ batchSummary.existing }}
        </span>
        <el-button :loading="batchPreviewing" @click="refreshBatchPreview">刷新预览</el-button>
        <el-button :disabled="!batchItems.length" @click="copyBatchCredentials">复制账号清单</el-button>
      </div>

      <el-table :data="batchItems" v-loading="batchPreviewing" max-height="360" size="small">
        <el-table-column prop="factoryName" label="工厂" width="110" />
        <el-table-column prop="username" label="用户名" width="100" />
        <el-table-column prop="password" label="初始密码" width="110" />
        <el-table-column label="角色" width="120">
          <template #default="{ row }">{{ formatRoleLabels(row.roles) }}</template>
        </el-table-column>
        <el-table-column label="状态" width="90">
          <template #default="{ row }">
            <el-tag :type="row.exists ? 'info' : 'success'" size="small">
              {{ row.exists ? '已存在' : '待创建' }}
            </el-tag>
          </template>
        </el-table-column>
      </el-table>

      <template #footer>
        <el-button @click="batchVisible = false">取消</el-button>
        <el-button
          type="primary"
          :loading="batchSubmitting"
          :disabled="!batchSummary.toCreate"
          @click="onBatchImport"
        >
          导入 {{ batchSummary.toCreate || 0 }} 个账号
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { onMounted, reactive, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { formatTime } from '../../utils/format'
import { ROLE_OPTIONS, formatRoleLabels } from '../../utils/roles'
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

const batchVisible = ref(false)
const batchPreviewing = ref(false)
const batchSubmitting = ref(false)
const batchItems = ref([])
const batchSummary = reactive({ total: 0, existing: 0, toCreate: 0 })
const batchForm = reactive({
  factoryCodes: [],
  roles: ['operator', 'engineer', 'admin'],
  stationKeys: [],
  skipExisting: true,
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
    const pwd = data?.password || data?.newPassword || 'ChangeMe123'
    ElMessage.success(`密码已重置为：${pwd}`)
    await load()
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  }
}

function copyAllPasswords() {
  const lines = items.value.map((row) => {
    const pwd = row.password || '（未知，请重置密码）'
    return `${row.username}\t${pwd}\t${formatRoleLabels(row.roles)}`
  })
  if (!lines.length) {
    ElMessage.warning('暂无账号')
    return
  }
  const text = ['用户名\t密码\t角色', ...lines].join('\n')
  navigator.clipboard.writeText(text).then(
    () => ElMessage.success('已复制到剪贴板'),
    () => ElMessage.warning('复制失败'),
  )
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

async function onDelete(row) {
  try {
    await ElMessageBox.confirm(
      `确认删除账号「${row.username}」？删除后不可恢复，登录审计中的历史记录会保留。`,
      '删除账号',
      { type: 'warning', confirmButtonText: '删除', cancelButtonText: '取消' },
    )
    await api.deleteUser(row.id)
    ElMessage.success('已删除')
    await load()
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  }
}

function openBatchImport() {
  Object.assign(batchForm, {
    factoryCodes: [],
    roles: ['operator', 'engineer', 'admin'],
    stationKeys: [],
    skipExisting: true,
  })
  batchItems.value = []
  Object.assign(batchSummary, { total: 0, existing: 0, toCreate: 0 })
  batchVisible.value = true
  meta.reloadFactories()
}

async function refreshBatchPreview() {
  batchPreviewing.value = true
  try {
    const data = await api.previewBatchUsers({
      factoryCodes: batchForm.factoryCodes.length ? batchForm.factoryCodes : null,
      roles: batchForm.roles,
    })
    batchItems.value = data?.items || []
    Object.assign(batchSummary, data?.summary || { total: 0, existing: 0, toCreate: 0 })
  } catch (e) {
    batchItems.value = []
    Object.assign(batchSummary, { total: 0, existing: 0, toCreate: 0 })
    ElMessage.error(e.message)
  } finally {
    batchPreviewing.value = false
  }
}

function copyBatchCredentials() {
  const lines = batchItems.value.map(
    (row) =>
      `${row.factoryName}\t${row.username}\t${row.password}\t${formatRoleLabels(row.roles)}${row.exists ? '\t(已存在)' : ''}`,
  )
  const text = ['工厂\t用户名\t密码\t角色', ...lines].join('\n')
  navigator.clipboard.writeText(text).then(
    () => ElMessage.success('已复制到剪贴板'),
    () => ElMessage.warning('复制失败，请手动选择表格内容'),
  )
}

async function onBatchImport() {
  if (!batchSummary.toCreate) {
    ElMessage.warning('没有可导入的新账号')
    return
  }
  try {
    await ElMessageBox.confirm(
      `确认导入 ${batchSummary.toCreate} 个新账号？已存在的账号将${batchForm.skipExisting ? '跳过' : '报错'}。`,
      '批量导入',
      { type: 'warning' },
    )
    batchSubmitting.value = true
    const data = await api.batchImportUsers({
      factoryCodes: batchForm.factoryCodes.length ? batchForm.factoryCodes : null,
      roles: batchForm.roles,
      stationKeys: batchForm.stationKeys,
      skipExisting: batchForm.skipExisting,
    })
    const created = data?.created || []
    if (created.length) {
      const detail = created
        .map((r) => `${r.username} / ${r.password}（${formatRoleLabels(r.roles)}）`)
        .join('\n')
      await ElMessageBox.alert(detail, `已创建 ${created.length} 个账号，请保存密码`, {
        confirmButtonText: '知道了',
      })
    } else {
      ElMessage.info('没有新账号被创建')
    }
    batchVisible.value = false
    await load()
  } catch (e) {
    if (e !== 'cancel') ElMessage.error(e.message)
  } finally {
    batchSubmitting.value = false
  }
}

onMounted(async () => {
  await meta.load()
  await load()
})
</script>

<style scoped>
.toolbar { margin-bottom: 16px; display: flex; gap: 8px; }
.role-group {
  display: flex;
  flex-wrap: wrap;
  gap: 8px 20px;
}
.role-item {
  display: inline-flex;
}
.batch-hint { margin-bottom: 16px; }
.batch-form { margin-bottom: 8px; }
.batch-toolbar {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 12px;
}
.batch-summary {
  flex: 1;
  font-size: 13px;
  color: #64748b;
}
.pwd-text {
  font-family: Consolas, 'Courier New', monospace;
  font-size: 13px;
}
.pwd-empty {
  color: #94a3b8;
}
</style>
