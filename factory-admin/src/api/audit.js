import http from './http'

export function listAuditLogins(params) {
  return http.get('/admin/audit-logins', { params })
}
