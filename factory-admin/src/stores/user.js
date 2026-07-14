import { defineStore } from 'pinia'
import http from '../api/http'

export const useUserStore = defineStore('user', {
  state: () => ({
    token: localStorage.getItem('fc_token') || '',
    username: localStorage.getItem('fc_username') || '',
    roles: JSON.parse(localStorage.getItem('fc_roles') || '[]'),
    stationKeys: JSON.parse(localStorage.getItem('fc_station_keys') || '[]'),
    factoryCode: localStorage.getItem('fc_factory_code') || '',
  }),
  actions: {
    _persistAuth() {
      localStorage.setItem('fc_token', this.token)
      localStorage.setItem('fc_username', this.username)
      localStorage.setItem('fc_roles', JSON.stringify(this.roles))
      localStorage.setItem('fc_station_keys', JSON.stringify(this.stationKeys))
      localStorage.setItem('fc_factory_code', this.factoryCode || '')
    },
    async login(username, password) {
      const data = await http.post('/auth/login', {
        username,
        password,
        hostName: 'web-console',
      })
      this.token = data.accessToken
      this.username = username
      this.roles = data.roles || []
      this.stationKeys = data.stationKeys || []
      this.factoryCode = data.factoryCode || ''
      this._persistAuth()
    },
    async refreshMe() {
      if (!this.token) return
      const data = await http.get('/auth/me')
      this.username = data.username || this.username
      this.roles = data.roles || []
      this.stationKeys = data.stationKeys || []
      this.factoryCode = data.factoryCode || ''
      this._persistAuth()
    },
    logout() {
      this.token = ''
      this.username = ''
      this.roles = []
      this.stationKeys = []
      this.factoryCode = ''
      localStorage.removeItem('fc_token')
      localStorage.removeItem('fc_username')
      localStorage.removeItem('fc_roles')
      localStorage.removeItem('fc_station_keys')
      localStorage.removeItem('fc_factory_code')
    },
  },
})
