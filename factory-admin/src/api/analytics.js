import http from './http'

export function getAnalyticsStations(params) {
  return http.get('/analytics/stations', { params })
}

export function getAnalyticsProducts(params) {
  return http.get('/analytics/products', { params })
}

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
