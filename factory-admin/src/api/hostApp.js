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

export function deleteVersion(params) {
  return http.delete('/admin/host-app/versions', { params })
}

export function getRuntimeEnvInfo() {
  return http.get('/admin/host-app/runtime-env/info')
}

export function downloadRuntimeEnv() {
  return http.get('/admin/host-app/runtime-env', {
    responseType: 'blob',
    timeout: 600000,
  }).then(async (data) => {
    // 失败时后端可能仍返回 JSON Blob，这里统一转成可读错误
    if (data instanceof Blob && data.type && data.type.includes('application/json')) {
      const text = await data.text()
      let message = '下载失败'
      try {
        const body = JSON.parse(text)
        message = body?.detail?.message || body?.message || message
      } catch {
        message = text || message
      }
      throw new Error(message)
    }
    return data
  })
}

export function uploadRuntimeEnv(formData) {
  return http.post('/admin/host-app/runtime-env', formData, {
    headers: { 'Content-Type': 'multipart/form-data' },
  })
}
