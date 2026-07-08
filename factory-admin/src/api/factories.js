import http from './http'

export function listAllFactories() {
  return http.get('/admin/meta/factories/all')
}

export function createFactory(body) {
  return http.post('/admin/meta/factories', body)
}

export function updateFactory(code, body) {
  return http.put(`/admin/meta/factories/${encodeURIComponent(code)}`, body)
}
