<template>
  <el-container class="layout">
    <el-aside :width="collapsed ? '64px' : '220px'" class="aside">
      <div class="logo" :class="{ collapsed }">
        <svg v-if="collapsed" viewBox="0 0 48 48" width="28" height="28">
          <rect x="6" y="20" width="8" height="20" rx="2" fill="var(--admin-primary)" opacity="0.5"/>
          <rect x="20" y="10" width="8" height="30" rx="2" fill="var(--admin-primary)" opacity="0.75"/>
          <rect x="34" y="14" width="8" height="26" rx="2" fill="var(--admin-primary)"/>
          <line x1="4" y1="42" x2="44" y2="42" stroke="var(--admin-primary)" stroke-width="2" stroke-linecap="round"/>
        </svg>
        <span v-else>路特产线管理平台</span>
      </div>

      <el-menu
        :default-active="activeMenu"
        router
        :collapse="collapsed"
        class="side-menu"
        background-color="transparent"
        :text-color="sidebarText"
        :active-text-color="sidebarTextActive"
      >
        <el-menu-item index="/dashboard">
          <el-icon><Odometer /></el-icon>
          <template #title>概览</template>
        </el-menu-item>

        <el-sub-menu index="data">
          <template #title>
            <el-icon><Search /></el-icon>
            <span>数据查询</span>
          </template>
          <el-menu-item index="/data/logs">
            <el-icon><Document /></el-icon>
            <template #title>日志查询</template>
          </el-menu-item>
          <el-menu-item index="/data/test-records">
            <el-icon><List /></el-icon>
            <template #title>测试数据</template>
          </el-menu-item>
        </el-sub-menu>

        <el-sub-menu index="analytics">
          <template #title>
            <el-icon><DataAnalysis /></el-icon>
            <span>数据分析</span>
          </template>
          <el-menu-item index="/data/curve">
            <el-icon><DataLine /></el-icon>
            <template #title>数据曲线</template>
          </el-menu-item>
          <el-menu-item index="/data/yield">
            <el-icon><Histogram /></el-icon>
            <template #title>良率统计</template>
          </el-menu-item>
        </el-sub-menu>

        <el-sub-menu v-if="isEngineer" index="config">
          <template #title>
            <el-icon><Setting /></el-icon>
            <span>配置管理</span>
          </template>
          <el-menu-item index="/config/test-cases">
            <el-icon><Files /></el-icon>
            <template #title>测试用例</template>
          </el-menu-item>
          <el-menu-item index="/config/host-app">
            <el-icon><Upload /></el-icon>
            <template #title>上位机版本管理</template>
          </el-menu-item>
        </el-sub-menu>

        <el-sub-menu v-if="isAdmin" index="system">
          <template #title>
            <el-icon><Monitor /></el-icon>
            <span>系统管理</span>
          </template>
          <el-menu-item index="/system/users">
            <el-icon><UserFilled /></el-icon>
            <template #title>账号管理</template>
          </el-menu-item>
          <el-menu-item index="/system/factories">
            <el-icon><OfficeBuilding /></el-icon>
            <template #title>工厂管理</template>
          </el-menu-item>
          <el-menu-item index="/system/devices">
            <el-icon><Tools /></el-icon>
            <template #title>设备登记</template>
          </el-menu-item>
          <el-menu-item index="/system/audit-logins">
            <el-icon><Clock /></el-icon>
            <template #title>登录审计</template>
          </el-menu-item>
          <el-menu-item index="/system/storage">
            <el-icon><Coin /></el-icon>
            <template #title>存储管理</template>
          </el-menu-item>
        </el-sub-menu>
      </el-menu>

      <div class="collapse-btn" @click="collapsed = !collapsed">
        <el-icon :class="{ rotated: collapsed }"><DArrowLeft /></el-icon>
      </div>
    </el-aside>

    <el-container>
      <el-header class="header">
        <div class="header-left">
          <span class="page-title">{{ pageTitle }}</span>
        </div>
        <div class="header-right">
          <el-dropdown trigger="click" @command="handleCommand">
            <span class="user-info">
              <el-avatar :size="32" class="user-avatar">{{ user.username.charAt(0).toUpperCase() }}</el-avatar>
              <span class="user-name">{{ user.username }}</span>
              <el-icon><ArrowDown /></el-icon>
            </span>
            <template #dropdown>
              <el-dropdown-menu>
                <el-dropdown-item command="profile">
                  <el-icon><User /></el-icon>个人中心
                </el-dropdown-item>
                <el-dropdown-item divided command="logout">
                  <el-icon><SwitchButton /></el-icon>退出登录
                </el-dropdown-item>
              </el-dropdown-menu>
            </template>
          </el-dropdown>
        </div>
      </el-header>

      <el-main class="main-content">
        <router-view />
      </el-main>
    </el-container>
  </el-container>
</template>

<script setup>
import { computed, ref, onMounted } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import { useUserStore } from '../stores/user'
import { useMetaStore } from '../stores/meta'
import { useRole } from '../composables/useRole'
import {
  Odometer, Search, Document, List, Setting, Files, Upload,
  Monitor, UserFilled, Tools, Clock, DArrowLeft, ArrowDown,
  User, SwitchButton, DataAnalysis, DataLine, Histogram, OfficeBuilding, Coin
} from '@element-plus/icons-vue'

const route = useRoute()
const router = useRouter()
const user = useUserStore()
const meta = useMetaStore()
const { isAdmin, isEngineer } = useRole()
const collapsed = ref(false)
const sidebarText = 'rgba(255, 255, 255, 0.78)'
const sidebarTextActive = '#ffffff'

const activeMenu = computed(() => route.meta.menu || route.path)
const pageTitle = computed(() => route.meta.title || '路特产线管理平台')

function handleCommand(cmd) {
  if (cmd === 'logout') {
    user.logout()
    router.push('/login')
  } else if (cmd === 'profile') {
    router.push('/profile')
  }
}

onMounted(async () => {
  await user.refreshMe()
  await meta.load()
})
</script>

<style scoped>
.layout {
  height: 100vh;
  background: var(--admin-bg);
}

.aside {
  background: var(--admin-sidebar);
  display: flex;
  flex-direction: column;
  transition: width 0.3s ease;
  overflow: hidden;
  border-right: 1px solid rgba(255, 255, 255, 0.06);
}

.logo {
  height: 56px;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #fff;
  font-size: 15px;
  font-weight: 600;
  letter-spacing: 0.5px;
  border-bottom: 1px solid rgba(255, 255, 255, 0.08);
  white-space: nowrap;
  overflow: hidden;
}
.logo.collapsed {
  padding: 0;
}

.side-menu {
  flex: 1;
  border-right: none;
  background: transparent;
  overflow-y: auto;
}

:deep(.el-menu),
:deep(.el-sub-menu .el-menu-item) {
  background: transparent;
}
:deep(.el-menu-item),
:deep(.el-sub-menu__title) {
  color: var(--admin-sidebar-text) !important;
}
:deep(.el-menu-item .el-icon),
:deep(.el-sub-menu__title .el-icon) {
  color: inherit !important;
}
:deep(.el-menu-item:hover),
:deep(.el-sub-menu__title:hover) {
  background: var(--admin-sidebar-hover);
  color: var(--admin-sidebar-text-active) !important;
}
:deep(.el-menu-item.is-active) {
  background: var(--admin-sidebar-active-bg) !important;
  color: var(--admin-sidebar-text-active) !important;
  border-right: 3px solid var(--admin-primary);
}
:deep(.el-sub-menu.is-opened > .el-sub-menu__title) {
  color: var(--admin-sidebar-text-active) !important;
}
:deep(.el-sub-menu .el-menu-item) {
  padding-left: 56px !important;
}
:deep(.el-menu--collapse .el-sub-menu .el-menu-item) {
  padding-left: 20px !important;
}

.collapse-btn {
  height: 44px;
  display: flex;
  align-items: center;
  justify-content: center;
  color: rgba(255, 255, 255, 0.45);
  cursor: pointer;
  border-top: 1px solid rgba(255, 255, 255, 0.08);
  transition: color 0.2s;
}
.collapse-btn:hover {
  color: #fff;
}
.collapse-btn .rotated {
  transform: rotate(180deg);
}

.header {
  height: 56px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  background: var(--admin-surface);
  border-bottom: 1px solid var(--admin-border);
  padding: 0 20px;
}

.page-title {
  font-size: 16px;
  font-weight: 600;
  color: var(--admin-text);
}

.header-right {
  display: flex;
  align-items: center;
}

.user-info {
  display: flex;
  align-items: center;
  gap: 8px;
  cursor: pointer;
  padding: 4px 8px;
  border-radius: var(--admin-radius);
  transition: background 0.2s;
}
.user-info:hover {
  background: var(--admin-border-light);
}

.user-avatar {
  background: var(--admin-primary);
  color: #fff;
  font-weight: 600;
  font-size: 14px;
}
.user-name {
  font-size: 14px;
  color: var(--admin-text);
  font-weight: 500;
}

.main-content {
  background: var(--admin-bg);
  padding: 20px;
  overflow-y: auto;
}
</style>
