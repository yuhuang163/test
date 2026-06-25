import { defineStore } from 'pinia'
import http from '../api/http'

const STATION_FALLBACK = [
  { key: 'FREE_WORK', name: '自由工站' },
  { key: 'PCBA', name: 'PCBA' },
  { key: 'AGING', name: '老化' },
  { key: 'PACK', name: '包装' },
]

export const useMetaStore = defineStore('meta', {
  state: () => ({
    factories: [],
    stations: [],
    loaded: false,
  }),
  actions: {
    async load() {
      if (this.loaded) return
      const tasks = [
        http.get('/admin/meta/factories').catch(() => []),
        http.get('/admin/meta/stations').catch(() => STATION_FALLBACK),
      ]
      const [factories, stations] = await Promise.all(tasks)
      this.factories = factories || []
      this.stations = stations?.length ? stations : STATION_FALLBACK
      this.loaded = true
    },
    factoryLabel(code) {
      const f = this.factories.find((x) => x.code === code)
      return f?.displayName || code || '-'
    },
    stationLabel(key) {
      const s = this.stations.find((x) => x.key === key)
      return s?.name || key || '-'
    },
  },
})
