import http from './http'

export function listTemplates(params) {
  return http.get('/admin/threshold-templates', { params })
}

export function getTemplate(id) {
  return http.get(`/admin/threshold-templates/${id}`)
}

export function createTemplate(body) {
  return http.post('/admin/threshold-templates', body)
}

export function updateTemplate(id, body) {
  return http.put(`/admin/threshold-templates/${id}`, body)
}

export function publishTemplate(id) {
  return http.post(`/admin/threshold-templates/${id}/publish`)
}
