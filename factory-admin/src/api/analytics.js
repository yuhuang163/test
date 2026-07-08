import http from './http'

export function getCurveData(params) {
  return http.get('/analytics/curve', { params })
}

export function getCurveItemNames(params) {
  return http.get('/analytics/curve/item-names', { params })
}

export function getYieldStats(params) {
  return http.get('/analytics/yield', { params })
}

export function getDashboardSummary() {
  return http.get('/analytics/dashboard')
}
