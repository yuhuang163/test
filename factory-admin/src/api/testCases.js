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

export function listVersions() {
  return http.get('/admin/test-cases/versions')
}

export function getVersionFiles(version) {
  return http.get(`/admin/test-cases/versions/${encodeURIComponent(version)}/files`)
}

export function getVersionFile(version, path) {
  return http.get(`/admin/test-cases/versions/${encodeURIComponent(version)}/files/${encodePath(path)}`)
}

export function diffVersions(fromVer, toVer) {
  return http.get('/admin/test-cases/versions/diff', { params: { from: fromVer, to: toVer } })
}

export function listStaging() {
  return http.get('/admin/test-cases/staging')
}

export function stagingDiff(params) {
  return http.get('/admin/test-cases/staging/diff', { params })
}

export function mergeStaging(body) {
  return http.post('/admin/test-cases/staging/merge', body)
}

export function clearStaging(params) {
  return http.delete('/admin/test-cases/staging', { params })
}

export function listMergeHistory(params) {
  return http.get('/admin/test-cases/merge-history', { params })
}

export function mergeHistoryDiff(mergeId) {
  return http.get(`/admin/test-cases/merge-history/${encodeURIComponent(mergeId)}/diff`)
}

export function undoMerge(mergeId) {
  return http.post(`/admin/test-cases/merge-history/${encodeURIComponent(mergeId)}/undo`)
}

export function listOnlineDevices() {
  return http.get('/admin/test-cases/online-devices')
}

export function pullProfile(body) {
  return http.post('/admin/test-cases/pull-profile', body)
}
