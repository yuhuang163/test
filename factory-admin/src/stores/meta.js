import { defineStore } from 'pinia'
import http from '../api/http'

const STATION_FALLBACK = [
  { key: 'FREE_WORK', name: '自由工站' },
  { key: 'PCBA', name: 'PCBA' },
  { key: 'AGING', name: '老化' },
  { key: 'PACK', name: '包装' },
]

const SETTINGS_KEY_FALLBACK = [
  'BLE/LowRssi',
  'BLE/HighRssi',
  'Current/LowCharCurrent',
  'Current/HighCharCurrent',
  'Current/LowmusicCurrent',
  'Current/HighmusicCurrent',
  'BATTARY/standbattary',
]

export const useMetaStore = defineStore('meta', {
  state: () => ({
    factories: [],
    stations: [],
    settingsKeys: [],
    loaded: false,
  }),
  actions: {
    async load() {
      if (this.loaded) return
      const tasks = [
        http.get('/admin/meta/factories').catch(() => []),
        http.get('/admin/meta/stations').catch(() => STATION_FALLBACK),
        http.get('/admin/meta/settings-keys').catch(() => SETTINGS_KEY_FALLBACK),
      ]
      const [factories, stations, settingsKeys] = await Promise.all(tasks)
      this.factories = factories || []
      this.stations = stations?.length ? stations : STATION_FALLBACK
      this.settingsKeys = settingsKeys?.length ? settingsKeys : SETTINGS_KEY_FALLBACK
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
