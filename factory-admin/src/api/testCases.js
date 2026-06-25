import http from './http'

function encodePath(path) {
  return path.split('/').map((seg) => encodeURIComponent(seg)).join('/')
}

export function listFiles() {
  return http.get('/admin/test-cases/files')
}

export function getFile(path) {
  return http.get(`/admin/test-cases/files/${encodePath(path)}`)
}

export function saveFile(path, content) {
  return http.put(`/admin/test-cases/files/${encodePath(path)}`, content, {
    headers: { 'Content-Type': 'text/plain; charset=utf-8' },
    transformRequest: [(data) => data],
  })
}

export function deleteFile(path) {
  return http.delete(`/admin/test-cases/files/${encodePath(path)}`)
}

export function downloadBundle() {
  return http.get('/admin/test-cases/bundle', { responseType: 'blob' })
}

export function publishBundle() {
  return http.post('/admin/test-cases/publish')
}
