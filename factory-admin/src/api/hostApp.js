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

export function getRuntimeEnvInfo() {
  return http.get('/admin/host-app/runtime-env/info')
}

export function downloadRuntimeEnv() {
  return http.get('/admin/host-app/runtime-env', { responseType: 'blob' })
}

export function uploadRuntimeEnv(formData) {
  return http.post('/admin/host-app/runtime-env', formData, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
}
