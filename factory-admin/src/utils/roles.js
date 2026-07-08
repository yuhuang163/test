/** 账号角色：value 与后端一致，label / tip 供界面展示。 */
export const ROLE_OPTIONS = [
  {
    value: 'operator',
    label: '产线操作员',
    tip: '可查看日志、测试数据与良率统计；上位机可登录并拉取用例与更新，不可上传发布。',
  },
  {
    value: 'engineer',
    label: '工艺工程师',
    tip: '在操作员权限基础上，可管理测试用例（网页编辑发布、上位机上传用例包及 exe）。',
  },
  {
    value: 'admin',
    label: '管理员',
    tip: '拥有全部权限：账号/设备/登录审计、上位机版本与运行环境管理；不受工站白名单限制。',
  },
]

const ROLE_LABEL_MAP = Object.fromEntries(ROLE_OPTIONS.map((r) => [r.value, r.label]))

/** 将角色 code 列表格式化为中文，用于表格与个人中心。 */
export function formatRoleLabels(roles, emptyText = '-') {
  const list = roles || []
  if (!list.length) return emptyText
  return list.map((r) => ROLE_LABEL_MAP[r] || r).join('、')
}
