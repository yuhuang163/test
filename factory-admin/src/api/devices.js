import http from './http'

export function listDevices(params) {
  return http.get('/admin/devices', { params })
}

export function createDevice(body) {
  return http.post('/admin/devices', body)
}

export function updateDevice(id, body) {
  return http.put(`/admin/devices/${id}`, body)
}

export function deleteDevice(id) {
  return http.delete(`/admin/devices/${id}`)
}
