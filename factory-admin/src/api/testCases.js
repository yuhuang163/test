import http from './http'

export function listFiles() {
  return http.get('/admin/test-cases/files')
}

export function getFile(path) {
  return http.get(`/admin/test-cases/files/${encodeURIComponent(path)}`)
}

export function saveFile(path, content) {
  return http.put(`/admin/test-cases/files/${encodeURIComponent(path)}`, content, {
    headers: { 'Content-Type': 'text/plain; charset=utf-8' },
    transformRequest: [(data) => data],
  })
}

export function publishBundle() {
  return http.post('/admin/test-cases/publish')
}
