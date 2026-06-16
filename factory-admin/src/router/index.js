import { createRouter, createWebHistory } from 'vue-router'
import { ElMessage } from 'element-plus'
import { useUserStore } from '../stores/user'
import LoginView from '../views/LoginView.vue'
import LayoutView from '../views/LayoutView.vue'
import DashboardView from '../views/DashboardView.vue'
import LogsView from '../views/LogsView.vue'
import LogDetailView from '../views/LogDetailView.vue'
import TestRecordsView from '../views/TestRecordsView.vue'
import ThresholdsView from '../views/thresholds/ThresholdsView.vue'
import ThresholdEditView from '../views/thresholds/ThresholdEditView.vue'
import TestCasesView from '../views/testCases/TestCasesView.vue'
import HostAppView from '../views/hostApp/HostAppView.vue'
import ReleasesView from '../views/releases/ReleasesView.vue'
import UsersView from '../views/system/UsersView.vue'
import DevicesView from '../views/system/DevicesView.vue'
import AuditLoginsView from '../views/system/AuditLoginsView.vue'
import ProfileView from '../views/ProfileView.vue'

const router = createRouter({
  history: createWebHistory(),
  routes: [
    { path: '/login', component: LoginView, meta: { public: true } },
    {
      path: '/',
      component: LayoutView,
      redirect: '/dashboard',
      children: [
        { path: 'dashboard', component: DashboardView, meta: { title: '概览' } },
        { path: 'data/logs', component: LogsView, meta: { title: '日志查询', menu: '/data/logs' } },
        { path: 'data/logs/:id', component: LogDetailView, meta: { title: '日志详情', menu: '/data/logs' } },
        { path: 'data/test-records', component: TestRecordsView, meta: { title: '测试数据', menu: '/data/test-records' } },
        {
          path: 'config/thresholds',
          component: ThresholdsView,
          meta: { title: '阈值模板', menu: '/config/thresholds', roles: ['engineer', 'admin'] },
        },
        {
          path: 'config/thresholds/:id',
          component: ThresholdEditView,
          meta: { title: '编辑阈值', menu: '/config/thresholds', roles: ['engineer', 'admin'] },
        },
        {
          path: 'config/test-cases',
          component: TestCasesView,
          meta: { title: '测试用例', menu: '/config/test-cases', roles: ['engineer', 'admin'] },
        },
        {
          path: 'config/host-app',
          component: HostAppView,
          meta: { title: '上位机版本', menu: '/config/host-app', roles: ['admin'] },
        },
        {
          path: 'config/releases',
          component: ReleasesView,
          meta: { title: '统一发布', menu: '/config/releases', roles: ['engineer', 'admin'] },
        },
        {
          path: 'system/users',
          component: UsersView,
          meta: { title: '账号管理', menu: '/system/users', roles: ['admin'] },
        },
        {
          path: 'system/devices',
          component: DevicesView,
          meta: { title: '设备登记', menu: '/system/devices', roles: ['admin'] },
        },
        {
          path: 'system/audit-logins',
          component: AuditLoginsView,
          meta: { title: '登录审计', menu: '/system/audit-logins', roles: ['admin'] },
        },
        { path: 'profile', component: ProfileView, meta: { title: '个人中心', menu: '/profile' } },
      ],
    },
  ],
})

function canAccess(roles, routeRoles) {
  if (!routeRoles?.length) return true
  if (roles.includes('admin')) return true
  return routeRoles.some((r) => roles.includes(r))
}

router.beforeEach((to) => {
  const user = useUserStore()
  if (to.meta.public) {
    if (to.path === '/login' && user.token) return '/dashboard'
    return true
  }
  if (!user.token) return '/login'
  if (to.meta.roles && !canAccess(user.roles || [], to.meta.roles)) {
    ElMessage.warning('无权限访问该页面')
    return '/dashboard'
  }
  return true
})

export default router
