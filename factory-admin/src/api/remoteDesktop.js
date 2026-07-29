import http from './http'

const RD_SESSION_KEY = 'fc_rd_session'

export function listRemoteDevices() {
  return http.get('/admin/remote-desktop/online-devices')
}

/** force=true：顶替该设备上旧会话（刷新残留） */
export function createRemoteSession(deviceId, { force = true, maxWidth, fps, maxBitrate } = {}) {
  const body = { deviceId, force }
  if (maxWidth != null) body.maxWidth = maxWidth
  if (fps != null) body.fps = fps
  if (maxBitrate != null) body.maxBitrate = maxBitrate
  return http.post('/admin/remote-desktop/sessions', body)
}

export function stopRemoteSession(sessionId) {
  return http.post(`/admin/remote-desktop/sessions/${sessionId}/stop`)
}

/** 刷新后丢失 sessionId 时，按设备断开 */
export function stopRemoteSessionByDevice(deviceId) {
  return http.post(`/admin/remote-desktop/devices/${encodeURIComponent(deviceId)}/stop-session`)
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

export function rememberSession(session) {
  try {
    if (!session?.sessionId) {
      sessionStorage.removeItem(RD_SESSION_KEY)
      return
    }
    sessionStorage.setItem(
      RD_SESSION_KEY,
      JSON.stringify({
        sessionId: session.sessionId,
        deviceId: session.deviceId || '',
      })
    )
  } catch (_) {
    /* ignore */
  }
}

export function clearRememberedSession() {
  try {
    sessionStorage.removeItem(RD_SESSION_KEY)
  } catch (_) {
    /* ignore */
  }
}

export function readRememberedSession() {
  try {
    const raw = sessionStorage.getItem(RD_SESSION_KEY)
    if (!raw) return null
    const obj = JSON.parse(raw)
    if (!obj?.sessionId) return null
    return obj
  } catch (_) {
    return null
  }
}

/** 刷新/关页时尽量通知后端停会话（不依赖 await） */
export function beaconStopSession(sessionId) {
  if (!sessionId) return
  const token = localStorage.getItem('fc_token') || ''
  const url = `/api/factory-tool/admin/remote-desktop/sessions/${encodeURIComponent(sessionId)}/stop`
  try {
    fetch(url, {
      method: 'POST',
      headers: {
        Authorization: token ? `Bearer ${token}` : '',
        'Content-Type': 'application/json',
      },
      body: '{}',
      keepalive: true,
      credentials: 'same-origin',
    }).catch(() => {})
  } catch (_) {
    /* ignore */
  }
}
