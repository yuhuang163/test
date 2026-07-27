import http from './http'

export function listRemoteDevices() {
  return http.get('/admin/remote-desktop/online-devices')
}

export function createRemoteSession(deviceId) {
  return http.post('/admin/remote-desktop/sessions', { deviceId })
}

export function stopRemoteSession(sessionId) {
  return http.post(`/admin/remote-desktop/sessions/${sessionId}/stop`)
}

export function getRemoteSession(sessionId) {
  return http.get(`/admin/remote-desktop/sessions/${sessionId}`)
}

/** 浏览器 WebSocket URL（同源 / 开发代理） */
export function buildViewerWsUrl(sessionId, viewerToken) {
  const proto = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
  const qs = new URLSearchParams({
    sessionId,
    role: 'viewer',
    token: viewerToken,
  })
  return `${proto}//${window.location.host}/api/factory-tool/remote-desktop/ws?${qs.toString()}`
}
