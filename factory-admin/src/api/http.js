import axios from 'axios'
import { ElMessage } from 'element-plus'
import { useUserStore } from '../stores/user'
import router from '../router'

const http = axios.create({
  baseURL: '/api/factory-tool',
  timeout: 120000,
})

http.interceptors.request.use((config) => {
  const user = useUserStore()
  if (user.token) {
    config.headers.Authorization = `Bearer ${user.token}`
  }
  return config
})

http.interceptors.response.use(
  (res) => {
    const ct = res.headers['content-type'] || ''
    if (typeof res.data === 'string' && (ct.includes('text/') || ct.includes('application/octet-stream'))) {
      return res.data
    }
    const body = res.data
    if (body && typeof body.code === 'number') {
      if (body.code === 0) return body.data
      return Promise.reject(new Error(body.message || '请求失败'))
    }
    return body
  },
  (err) => {
    const status = err.response?.status
    const detail = err.response?.data
    let msg = ''
    // blob 下载失败时 data 常为 Blob，需异步解析；这里同步尽量取可读文案
    if (detail && typeof detail === 'object' && !(detail instanceof Blob)) {
      msg = detail.message || detail.detail?.message || (typeof detail.detail === 'string' ? detail.detail : '')
    } else if (typeof detail === 'string') {
      msg = detail
    }
    if (!msg) {
      msg = err.message
    }
    if (status === 401) {
      const user = useUserStore()
      user.logout()
      router.push('/login')
    } else if (status === 403) {
      ElMessage.error(msg || '无权限')
    } else if (status === 404) {
      ElMessage.error(msg || '接口未就绪，请确认后端已部署对应功能')
    } else if (status === 405) {
      ElMessage.error(msg || '请求方法不被允许，请重启后端并刷新管理端页面后重试')
    }
    return Promise.reject(new Error(msg || '请求失败'))
  }
)

export default http
