import http from './http'

async function asDownloadBlob(data) {
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
}

export function getDownloadSummary(params) {
  return http.get('/downloads/summary', { params })
}

export function downloadHostExe(params) {
  return http
    .get('/downloads/host-exe', {
      params,
      responseType: 'blob',
      timeout: 600000,
    })
    .then(asDownloadBlob)
}

export function downloadTestCases() {
  return http
    .get('/downloads/test-cases', {
      responseType: 'blob',
      timeout: 600000,
    })
    .then(asDownloadBlob)
}

export function downloadRuntimeEnv() {
  return http
    .get('/downloads/runtime-env', {
      responseType: 'blob',
      timeout: 600000,
    })
    .then(asDownloadBlob)
}
