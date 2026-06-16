import http from './http'

export function listUsers(params) {
  return http.get('/admin/users', { params })
}

export function createUser(body) {
  return http.post('/admin/users', body)
}

export function updateUser(id, body) {
  return http.put(`/admin/users/${id}`, body)
}

export function resetPassword(id) {
  return http.post(`/admin/users/${id}/reset-password`)
}

export function unlockUser(id) {
  return http.post(`/admin/users/${id}/unlock`)
}

export function changePassword(body) {
  return http.post('/auth/change-password', body)
}
