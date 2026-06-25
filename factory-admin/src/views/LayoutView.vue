<template>

  <el-container class="layout">

    <el-aside width="220px" class="aside">

      <div class="logo">路特产线管理平台</div>

      <el-menu :default-active="activeMenu" router class="side-menu">

        <el-menu-item index="/dashboard">概览</el-menu-item>



        <el-sub-menu index="data">

          <template #title>数据查询</template>

          <el-menu-item index="/data/logs">日志查询</el-menu-item>
          <el-menu-item index="/data/test-records">测试数据</el-menu-item>

        </el-sub-menu>



        <el-sub-menu v-if="isEngineer" index="config">

          <template #title>配置管理</template>

          <el-menu-item index="/config/test-cases">测试用例</el-menu-item>

          <el-menu-item index="/config/releases">统一发布</el-menu-item>

          <el-menu-item v-if="isAdmin" index="/config/host-app">上位机版本</el-menu-item>

        </el-sub-menu>



        <el-sub-menu v-if="isAdmin" index="system">

          <template #title>系统管理</template>

          <el-menu-item index="/system/users">账号管理</el-menu-item>

          <el-menu-item index="/system/devices">设备登记</el-menu-item>

          <el-menu-item index="/system/audit-logins">登录审计</el-menu-item>

        </el-sub-menu>

      </el-menu>

    </el-aside>

    <el-container>

      <el-header class="header">

        <span class="title">{{ pageTitle }}</span>

        <div class="right">

          <span class="user">{{ user.username }}</span>

          <el-button link type="primary" @click="router.push('/profile')">个人中心</el-button>

          <el-button link type="primary" @click="onLogout">退出</el-button>

        </div>

      </el-header>

      <el-main>

        <router-view />

      </el-main>

    </el-container>

  </el-container>

</template>



<script setup>

import { computed, onMounted } from 'vue'

import { useRoute, useRouter } from 'vue-router'

import { useUserStore } from '../stores/user'

import { useMetaStore } from '../stores/meta'

import { useRole } from '../composables/useRole'



const route = useRoute()

const router = useRouter()

const user = useUserStore()

const meta = useMetaStore()

const { isAdmin, isEngineer } = useRole()



const activeMenu = computed(() => route.meta.menu || route.path)

const pageTitle = computed(() => route.meta.title || '路特产线管理平台')



function onLogout() {

  user.logout()

  router.push('/login')

}



onMounted(() => meta.load())

</script>



<style scoped>

.layout { height: 100vh; }

.aside { background: #001529; color: #fff; display: flex; flex-direction: column; }

.logo {

  height: 56px;

  line-height: 56px;

  text-align: center;

  font-weight: bold;

  color: #fff;

  font-size: 14px;

  padding: 0 8px;

  border-bottom: 1px solid #0d2847;

}

.header {

  display: flex;

  align-items: center;

  justify-content: space-between;

  border-bottom: 1px solid #eee;

  height: 56px;

}

.title { font-size: 16px; font-weight: 600; }

.right { display: flex; align-items: center; gap: 8px; }

.user { color: #666; margin-right: 4px; }

.side-menu { flex: 1; border-right: none; background: #001529; }

:deep(.el-menu) { background: #001529; }

:deep(.el-menu-item),

:deep(.el-sub-menu__title) { color: rgba(255, 255, 255, 0.85); }

:deep(.el-menu-item.is-active) { background: #1890ff !important; color: #fff; }

:deep(.el-sub-menu .el-menu-item) { background: #000c17; }

</style>

