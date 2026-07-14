import { createRouter, createWebHistory } from 'vue-router'
import { ElMessage } from 'element-plus'
import { useUserStore } from '../stores/user'
import LoginView from '../views/LoginView.vue'
import LayoutView from '../views/LayoutView.vue'
import DashboardView from '../views/DashboardView.vue'
import LogsView from '../views/LogsView.vue'
import LogDetailView from '../views/LogDetailView.vue'
import TestRecordsView from '../views/TestRecordsView.vue'
import DataCurveView from '../views/DataCurveView.vue'
import YieldView from '../views/YieldView.vue'
import TestCasesView from '../views/testCases/TestCasesView.vue'
import VersionHistoryView from '../views/testCases/VersionHistoryView.vue'
import HostAppView from '../views/hostApp/HostAppView.vue'
import UsersView from '../views/system/UsersView.vue'
import FactoriesView from '../views/system/FactoriesView.vue'
import DevicesView from '../views/system/DevicesView.vue'
import AuditLoginsView from '../views/system/AuditLoginsView.vue'
import StorageView from '../views/system/StorageView.vue'
import DownloadCenterView from '../views/DownloadCenterView.vue'
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
        {
          path: 'downloads',
          component: DownloadCenterView,
          meta: { title: '上位机下载', menu: '/downloads' },
        },
        { path: 'data/logs', component: LogsView, meta: { title: '日志查询', menu: '/data/logs' } },
        { path: 'data/logs/:id', component: LogDetailView, meta: { title: '日志详情', menu: '/data/logs' } },
        { path: 'data/test-records', component: TestRecordsView, meta: { title: '测试数据', menu: '/data/test-records' } },
        { path: 'data/curve', component: DataCurveView, meta: { title: '数据曲线', menu: '/data/curve' } },
        { path: 'data/yield', component: YieldView, meta: { title: '良率统计', menu: '/data/yield' } },
        {
          path: 'config/test-cases',
          component: TestCasesView,
          meta: { title: '测试用例', menu: '/config/test-cases', roles: ['engineer', 'admin'] },
        },
        {
          path: 'config/test-cases/versions',
          component: VersionHistoryView,
          meta: { title: '版本历史', menu: '/config/test-cases', roles: ['engineer', 'admin'] },
        },
        {
          path: 'config/host-app',
          component: HostAppView,
          meta: { title: '上位机版本管理', menu: '/config/host-app', roles: ['engineer', 'admin'] },
        },
        {
          path: 'system/users',
          component: UsersView,
          meta: { title: '账号管理', menu: '/system/users', roles: ['admin'] },
        },
        {
          path: 'system/factories',
          component: FactoriesView,
          meta: { title: '工厂管理', menu: '/system/factories', roles: ['admin'] },
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
        {
          path: 'system/storage',
          component: StorageView,
          meta: { title: '存储管理', menu: '/system/storage', roles: ['admin'] },
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
