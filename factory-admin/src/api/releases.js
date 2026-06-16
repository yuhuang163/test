import http from './http'

export function listReleases(params) {
  return http.get('/admin/releases', { params })
}

export function createRelease(body) {
  return http.post('/admin/releases', body)
}
