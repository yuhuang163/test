import http from './http'

export function getStorageInfo() {
  return http.get('/admin/storage/info')
}
