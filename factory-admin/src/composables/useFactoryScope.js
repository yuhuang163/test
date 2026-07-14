import { computed } from 'vue'
import { useUserStore } from '../stores/user'
import { useMetaStore } from '../stores/meta'

/** 工厂账号仅能见本厂；平台 admin 的 factoryCode 为空。 */
export function useFactoryScope() {
  const user = useUserStore()
  const meta = useMetaStore()

  const scopedFactoryCode = computed(() => user.factoryCode || '')
  const isFactoryScoped = computed(() => !!scopedFactoryCode.value)
  const scopedFactoryLabel = computed(() => meta.factoryLabel(scopedFactoryCode.value))

  function applyScopedFactoryFilter(filters) {
    if (scopedFactoryCode.value) {
      filters.factoryName = scopedFactoryCode.value
    }
  }

  return {
    scopedFactoryCode,
    isFactoryScoped,
    scopedFactoryLabel,
    applyScopedFactoryFilter,
  }
}
