import http from './http'

export function listVersions(params) {
  return http.get('/admin/host-app/versions', { params })
}

export function createVersion(body) {
  return http.post('/admin/host-app/versions', body)
}

export function uploadVersion(formData) {
  return http.post('/admin/host-app/versions', formData, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
}
