import { computed } from 'vue'
import { useUserStore } from '../stores/user'

export function useRole() {
  const user = useUserStore()
  const roles = computed(() => user.roles || [])

  const isAdmin = computed(() => roles.value.includes('admin'))
  const isEngineer = computed(() => roles.value.includes('engineer') || isAdmin.value)
  const isOperator = computed(() => roles.value.includes('operator') || isEngineer.value)

  function canAccess(routeRoles) {
    if (!routeRoles?.length) return true
    if (isAdmin.value) return true
    return routeRoles.some((r) => roles.value.includes(r))
  }

  return { roles, isAdmin, isEngineer, isOperator, canAccess }
}
