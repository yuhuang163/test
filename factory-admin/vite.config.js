import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

// 与 scripts/port.bat 中 FACTORY_API_PORT 保持一致
const API_PORT = 8800

export default defineConfig({
  plugins: [vue()],
  server: {
    host: '127.0.0.1',
    port: 5173,
    strictPort: true,
    proxy: {
      '/api': {
        target: `http://127.0.0.1:${API_PORT}`,
        changeOrigin: true,
      },
    },
  },
})
