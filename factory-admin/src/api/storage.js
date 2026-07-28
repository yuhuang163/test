import http from './http'

export function getStorageInfo() {
  return http.get('/admin/storage/info')
}

export function listStorageHosts() {
  return http.get('/admin/storage/hosts')
}

export function deleteStorageHostData(body) {
  return http.post('/admin/storage/hosts/delete', body)
}
