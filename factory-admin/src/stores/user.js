import { defineStore } from 'pinia'
import http from '../api/http'

export const useUserStore = defineStore('user', {
  state: () => ({
    token: localStorage.getItem('fc_token') || '',
    username: localStorage.getItem('fc_username') || '',
    roles: JSON.parse(localStorage.getItem('fc_roles') || '[]'),
    stationKeys: JSON.parse(localStorage.getItem('fc_station_keys') || '[]'),
  }),
  actions: {
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
      localStorage.setItem('fc_token', this.token)
      localStorage.setItem('fc_username', this.username)
      localStorage.setItem('fc_roles', JSON.stringify(this.roles))
      localStorage.setItem('fc_station_keys', JSON.stringify(this.stationKeys))
    },
    logout() {
      this.token = ''
      this.username = ''
      this.roles = []
      this.stationKeys = []
      localStorage.removeItem('fc_token')
      localStorage.removeItem('fc_username')
      localStorage.removeItem('fc_roles')
      localStorage.removeItem('fc_station_keys')
    },
  },
})
