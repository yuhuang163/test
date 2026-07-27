<template>
  <div class="rd-page" :class="{ 'is-expanded': expanded }">
    <el-alert
      v-show="!expanded"
      class="rd-tip"
      type="info"
      :closable="false"
      show-icon
      title="联调说明：本页列出的是「当前管理端所连 API」上的在线心跳设备。"
      description="若上位机设置里 BaseUrl 指向其他地址（例如 192.168.x.x:8080 或正式服），此处会一直为空。本地测试请把上位机 BaseUrl 改为 http://127.0.0.1:8800 并重新登录云端。"
    />

    <div class="toolbar">
      <el-select
        v-model="selectedDeviceId"
        filterable
        clearable
        placeholder="选择在线产线机"
        style="width: 280px"
        :disabled="!!session"
        :loading="loadingDevices"
      >
        <el-option
          v-for="d in devices"
          :key="d.deviceId"
          :label="deviceLabel(d)"
          :value="d.deviceId"
          :disabled="d.remoteDesktop === false"
        >
          <span>{{ deviceLabel(d) }}</span>
          <el-tag v-if="d.remoteDesktop === false" size="small" type="info" class="opt-tag">无Agent</el-tag>
        </el-option>
        <template #empty>
          <div class="empty-tip">{{ emptyHint }}</div>
        </template>
      </el-select>
      <el-button @click="loadDevices" :loading="loadingDevices">刷新</el-button>
      <el-button
        type="primary"
        :disabled="!selectedDeviceId || !!session"
        :loading="starting"
        @click="startSession"
      >
        开始远控
      </el-button>
      <el-button type="danger" :disabled="!session" :loading="stopping" @click="confirmEndSession">
        断开
      </el-button>

      <el-divider direction="vertical" />

      <el-button :disabled="!session" @click="toggleExpanded">
        {{ expanded ? '还原' : '放大' }}
      </el-button>
      <el-button :disabled="!session" @click="toggleBrowserFullscreen">
        {{ isBrowserFs ? '退出全屏' : '全屏' }}
      </el-button>
      <el-select v-model="scaleMode" size="default" style="width: 110px" :disabled="!session">
        <el-option label="适应" value="contain" />
        <el-option label="铺满" value="cover" />
        <el-option label="拉伸" value="fill" />
      </el-select>

      <el-divider direction="vertical" />

      <el-dropdown :disabled="!session || !streamReady" @command="sendSpecial">
        <el-button :disabled="!session || !streamReady">
          特殊键
          <el-icon class="el-icon--right"><ArrowDown /></el-icon>
        </el-button>
        <template #dropdown>
          <el-dropdown-menu>
            <el-dropdown-item command="win">Win</el-dropdown-item>
            <el-dropdown-item command="ctrl_esc">Ctrl+Esc（开始菜单）</el-dropdown-item>
            <el-dropdown-item command="alt_tab">Alt+Tab</el-dropdown-item>
            <el-dropdown-item command="ctrl_shift_esc">Ctrl+Shift+Esc（任务管理器）</el-dropdown-item>
            <el-dropdown-item command="cad">Ctrl+Alt+Del（改发任务管理器）</el-dropdown-item>
            <el-dropdown-item divided command="win_l">Win+L 锁屏</el-dropdown-item>
            <el-dropdown-item command="win_d">Win+D 显示桌面</el-dropdown-item>
            <el-dropdown-item command="win_e">Win+E 资源管理器</el-dropdown-item>
            <el-dropdown-item command="alt_f4">Alt+F4</el-dropdown-item>
          </el-dropdown-menu>
        </template>
      </el-dropdown>
      <el-button :disabled="!session || !streamReady" @click="pasteClipboard">粘贴到远端</el-button>
      <el-button :disabled="!session || !streamReady" @click="takeScreenshot">截图</el-button>
      <el-button @click="helpVisible = true">快捷键</el-button>

      <span v-if="statusText" class="status">{{ statusText }}</span>
      <span class="device-count">在线 {{ devices.length }} 台</span>
    </div>

    <div
      ref="stageRef"
      class="stage"
      :class="{ expanded }"
      tabindex="0"
      @mousemove="onMouseMove"
      @mousedown="onMouseDown"
      @mouseup="onMouseUp"
      @wheel.prevent="onWheel"
      @keydown="onKeyDown"
      @keyup="onKeyUp"
      @dblclick="toggleExpanded"
      @contextmenu.prevent
    >
      <video
        ref="videoRef"
        class="screen"
        :style="{ objectFit: scaleMode }"
        autoplay
        playsinline
        muted
      />
      <div v-if="!session" class="placeholder">选择在线设备后点击「开始远控」</div>
      <div v-else-if="!streamReady" class="placeholder">{{ waitHint }}</div>
      <div v-if="session && streamReady" class="stage-hint">
        Esc 退出 · 双击放大 · F11 全屏 · F1 快捷键
      </div>
    </div>

    <el-drawer v-model="helpVisible" title="远控快捷键与功能" size="420px">
      <el-descriptions :column="1" border size="small">
        <el-descriptions-item label="Esc">退出全屏 → 还原放大 → 断开远控</el-descriptions-item>
        <el-descriptions-item label="F11">浏览器全屏 / 退出</el-descriptions-item>
        <el-descriptions-item label="F1">打开本说明</el-descriptions-item>
        <el-descriptions-item label="双击画面">放大 / 还原</el-descriptions-item>
        <el-descriptions-item label="适应/铺满/拉伸">画面缩放模式</el-descriptions-item>
        <el-descriptions-item label="特殊键">Win / Alt+Tab / 任务管理器等</el-descriptions-item>
        <el-descriptions-item label="粘贴到远端">把本机剪贴板文字发到产线机 Ctrl+V</el-descriptions-item>
        <el-descriptions-item label="截图">保存当前远控画面为 PNG</el-descriptions-item>
      </el-descriptions>
      <p class="help-note">
        说明：Windows 禁止普通程序注入真正的 Ctrl+Alt+Del，菜单项会改为打开任务管理器。
        键鼠需点击画面使其获焦后生效。
      </p>
    </el-drawer>
  </div>
</template>

<script setup>
import { computed, onBeforeUnmount, onMounted, ref } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { ArrowDown } from '@element-plus/icons-vue'
import * as api from '../api/remoteDesktop'
import * as testCaseApi from '../api/testCases'

const devices = ref([])
const loadingDevices = ref(false)
const loadError = ref('')
const selectedDeviceId = ref('')
const starting = ref(false)
const stopping = ref(false)
const session = ref(null)
const statusText = ref('')
const streamReady = ref(false)
const waitSeconds = ref(0)
const expanded = ref(false)
const isBrowserFs = ref(false)
const scaleMode = ref('contain')
const helpVisible = ref(false)

const emptyHint = computed(() => {
  if (loadError.value) return loadError.value
  return '暂无在线心跳。请确认：① 上位机已登录；② BaseUrl 指向本管理端 API（本地一般为 http://127.0.0.1:8800）；③ 心跳间隔内保持运行'
})

const waitHint = computed(() => {
  const s = waitSeconds.value
  if (s < 5) return `正在通知产线拉起 Agent…（${s}s）`
  if (s < 15) return `等待 Agent 推流…（${s}s，命令轮询约 3 秒）`
  return `仍在等待推流（${s}s）。请看上位机日志 [RemoteDesktop]，并确认 bin/remote_agent 已装好依赖（先手动跑一次 run.bat）`
})

const videoRef = ref(null)
const stageRef = ref(null)

let pc = null
let ws = null
let inputChannel = null
let screenWidth = 1920
let screenHeight = 1080
let lastMoveSent = 0
let refreshTimer = null
let waitTimer = null

function startWaitTimer() {
  waitSeconds.value = 0
  if (waitTimer) clearInterval(waitTimer)
  waitTimer = setInterval(() => {
    waitSeconds.value += 1
  }, 1000)
}

function stopWaitTimer() {
  if (waitTimer) {
    clearInterval(waitTimer)
    waitTimer = null
  }
}

function toggleExpanded() {
  if (!session.value) return
  expanded.value = !expanded.value
  requestAnimationFrame(() => stageRef.value?.focus())
}

async function toggleBrowserFullscreen() {
  const el = stageRef.value
  if (!el || !session.value) return
  try {
    if (!document.fullscreenElement) {
      expanded.value = true
      await el.requestFullscreen()
      isBrowserFs.value = true
      stageRef.value?.focus()
    } else {
      await document.exitFullscreen()
      isBrowserFs.value = false
    }
  } catch (e) {
    ElMessage.warning(e.message || '当前浏览器不支持全屏')
  }
}

function onFullscreenChange() {
  isBrowserFs.value = !!document.fullscreenElement
  if (!document.fullscreenElement) {
    expanded.value = false
  }
}

async function handleEsc() {
  if (!session.value) return
  if (document.fullscreenElement) {
    try {
      await document.exitFullscreen()
    } catch (_) {
      /* ignore */
    }
    return
  }
  if (expanded.value) {
    expanded.value = false
    return
  }
  await confirmEndSession()
}

async function confirmEndSession() {
  if (!session.value) return
  if (streamReady.value) {
    try {
      await ElMessageBox.confirm('确定断开远控？', '退出远控', {
        type: 'warning',
        confirmButtonText: '断开',
        cancelButtonText: '取消',
      })
    } catch {
      return
    }
  }
  await endSession()
}

function deviceLabel(d) {
  const host = d.hostName || d.deviceId
  const st = d.stationName || d.stationKey
  return st ? `${host}（${st}）` : host
}

async function loadDevices() {
  loadingDevices.value = true
  loadError.value = ''
  try {
    let items = []
    try {
      const data = await api.listRemoteDevices()
      items = data?.items || []
    } catch (e1) {
      const data = await testCaseApi.listOnlineDevices()
      items = data?.items || []
      if (!items.length && e1?.message) loadError.value = e1.message
    }
    devices.value = items
  } catch (e) {
    devices.value = []
    loadError.value = e.message || '加载在线设备失败（请确认已登录管理端且 API 已重启）'
    ElMessage.error(loadError.value)
  } finally {
    loadingDevices.value = false
  }
}

function cleanupPeer() {
  try {
    inputChannel?.close()
  } catch (_) {
    /* ignore */
  }
  inputChannel = null
  try {
    pc?.close()
  } catch (_) {
    /* ignore */
  }
  pc = null
  if (videoRef.value) videoRef.value.srcObject = null
  streamReady.value = false
}

function cleanupWs() {
  try {
    ws?.close()
  } catch (_) {
    /* ignore */
  }
  ws = null
}

async function endSession() {
  if (!session.value) return
  stopping.value = true
  const sid = session.value.sessionId
  try {
    try {
      ws?.send(JSON.stringify({ type: 'hangup', reason: 'viewer_hangup' }))
    } catch (_) {
      /* ignore */
    }
    await api.stopRemoteSession(sid)
  } catch (e) {
    ElMessage.warning(e.message || '停止会话失败')
  } finally {
    if (document.fullscreenElement) {
      try {
        await document.exitFullscreen()
      } catch (_) {
        /* ignore */
      }
    }
    expanded.value = false
    cleanupWs()
    cleanupPeer()
    session.value = null
    statusText.value = '已断开'
    stopWaitTimer()
    stopping.value = false
  }
}

function sendInput(evt) {
  const body = JSON.stringify(evt)
  // 优先 DataChannel；不通则走信令 WebSocket（画面能看但点不动时的兜底）
  if (inputChannel && inputChannel.readyState === 'open') {
    try {
      inputChannel.send(body)
      return
    } catch (_) {
      /* fall through */
    }
  }
  if (ws && ws.readyState === WebSocket.OPEN) {
    try {
      ws.send(JSON.stringify({ type: 'input', data: evt }))
    } catch (_) {
      /* ignore */
    }
  }
}

function normPos(e) {
  const el = videoRef.value
  if (!el) return { x: 0, y: 0 }
  const rect = el.getBoundingClientRect()
  const vw = el.videoWidth || screenWidth
  const vh = el.videoHeight || screenHeight
  if (!vw || !vh || rect.width <= 0 || rect.height <= 0) return { x: 0, y: 0 }

  // object-fit: contain/cover 时画面有黑边或裁剪，需映射到内容区
  const mode = scaleMode.value || 'contain'
  let contentW = rect.width
  let contentH = rect.height
  let offsetX = 0
  let offsetY = 0
  if (mode === 'contain') {
    const scale = Math.min(rect.width / vw, rect.height / vh)
    contentW = vw * scale
    contentH = vh * scale
    offsetX = (rect.width - contentW) / 2
    offsetY = (rect.height - contentH) / 2
  } else if (mode === 'cover') {
    const scale = Math.max(rect.width / vw, rect.height / vh)
    contentW = vw * scale
    contentH = vh * scale
    offsetX = (rect.width - contentW) / 2
    offsetY = (rect.height - contentH) / 2
  }

  const x = contentW > 0 ? (e.clientX - rect.left - offsetX) / contentW : 0
  const y = contentH > 0 ? (e.clientY - rect.top - offsetY) / contentH : 0
  return {
    x: Math.min(1, Math.max(0, x)),
    y: Math.min(1, Math.max(0, y)),
  }
}

function sendSpecial(name) {
  sendInput({ type: 'special', name })
  ElMessage.success(`已发送：${name}`)
  stageRef.value?.focus()
}

async function pasteClipboard() {
  try {
    const text = await navigator.clipboard.readText()
    if (!text) {
      ElMessage.warning('本机剪贴板没有文字')
      return
    }
    // 限制过大文本
    const clipped = text.length > 20000 ? text.slice(0, 20000) : text
    sendInput({ type: 'paste', text: clipped })
    ElMessage.success('已粘贴到远端')
    stageRef.value?.focus()
  } catch (e) {
    ElMessage.error(e.message || '读取剪贴板失败（需 HTTPS 或本机授权）')
  }
}

function takeScreenshot() {
  const video = videoRef.value
  if (!video || !video.videoWidth) {
    ElMessage.warning('暂无画面可截图')
    return
  }
  const canvas = document.createElement('canvas')
  canvas.width = video.videoWidth
  canvas.height = video.videoHeight
  const ctx = canvas.getContext('2d')
  ctx.drawImage(video, 0, 0)
  canvas.toBlob((blob) => {
    if (!blob) return
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    const ts = new Date().toISOString().replace(/[:.]/g, '-')
    a.href = url
    a.download = `remote_${session.value?.deviceId || 'screen'}_${ts}.png`
    a.click()
    URL.revokeObjectURL(url)
    ElMessage.success('截图已下载')
  }, 'image/png')
}

function onMouseMove(e) {
  if (!session.value) return
  const now = Date.now()
  if (now - lastMoveSent < 33) return
  lastMoveSent = now
  const p = normPos(e)
  sendInput({ type: 'mousemove', x: p.x, y: p.y })
}

function onMouseDown(e) {
  if (!session.value) return
  e.preventDefault()
  stageRef.value?.focus()
  const p = normPos(e)
  sendInput({ type: 'mousedown', x: p.x, y: p.y, button: e.button })
}

function onMouseUp(e) {
  if (!session.value) return
  e.preventDefault()
  const p = normPos(e)
  sendInput({ type: 'mouseup', x: p.x, y: p.y, button: e.button })
}

function onWheel(e) {
  if (!session.value) return
  const p = normPos(e)
  sendInput({ type: 'wheel', x: p.x, y: p.y, deltaY: e.deltaY })
}

function onKeyDown(e) {
  if (!session.value) return
  // 本页快捷键优先
  if (e.key === 'Escape') {
    e.preventDefault()
    e.stopPropagation()
    handleEsc()
    return
  }
  if (e.key === 'F1') {
    e.preventDefault()
    helpVisible.value = true
    return
  }
  if (e.key === 'F11') {
    e.preventDefault()
    toggleBrowserFullscreen()
    return
  }
  e.preventDefault()
  sendInput({ type: 'keydown', vk: e.keyCode || e.which })
}

function onKeyUp(e) {
  if (!session.value) return
  if (e.key === 'Escape' || e.key === 'F1' || e.key === 'F11') {
    e.preventDefault()
    return
  }
  e.preventDefault()
  sendInput({ type: 'keyup', vk: e.keyCode || e.which })
}

function attachDataChannel(ch) {
  inputChannel = ch
  ch.onopen = () => {
    statusText.value = streamReady.value ? '已连接（可键鼠控制）' : '键鼠通道已开，等待画面…'
  }
  ch.onclose = () => {
    if (inputChannel === ch) inputChannel = null
  }
}

async function handleOffer(msg) {
  cleanupPeer()
  screenWidth = Number(msg.screenWidth) || screenWidth
  screenHeight = Number(msg.screenHeight) || screenHeight

  pc = new RTCPeerConnection({ iceServers: session.value?.iceServers || [] })
  pc.ondatachannel = (ev) => {
    if (ev.channel) attachDataChannel(ev.channel)
  }
  // 应答端再建一条，提高键鼠通道成功率
  attachDataChannel(pc.createDataChannel('input-viewer'))
  pc.ontrack = (ev) => {
    if (videoRef.value) {
      videoRef.value.srcObject = ev.streams[0] || new MediaStream([ev.track])
      streamReady.value = true
      stopWaitTimer()
      const dcOk = inputChannel && inputChannel.readyState === 'open'
      statusText.value = dcOk ? '已连接（可键鼠控制）' : '画面已到达（键鼠走信令）'
      requestAnimationFrame(() => stageRef.value?.focus())
    }
  }
  pc.onicecandidate = (ev) => {
    if (!ev.candidate || !ws || ws.readyState !== WebSocket.OPEN) return
    ws.send(
      JSON.stringify({
        type: 'ice',
        candidate: {
          candidate: ev.candidate.candidate,
          sdpMid: ev.candidate.sdpMid,
          sdpMLineIndex: ev.candidate.sdpMLineIndex,
        },
      })
    )
  }

  await pc.setRemoteDescription({ type: 'offer', sdp: msg.sdp })
  const answer = await pc.createAnswer()
  await pc.setLocalDescription(answer)
  ws.send(JSON.stringify({ type: 'answer', sdp: pc.localDescription.sdp }))
}

async function startSession() {
  if (!selectedDeviceId.value) return
  starting.value = true
  statusText.value = '创建会话…'
  try {
    const data = await api.createRemoteSession(selectedDeviceId.value)
    session.value = data
    streamReady.value = false
    statusText.value = '等待 Agent…'
    startWaitTimer()

    const url = api.buildViewerWsUrl(data.sessionId, data.viewerToken)
    ws = new WebSocket(url)
    ws.onopen = () => {
      statusText.value = '信令已连接，等待推流…'
    }
    ws.onmessage = async (ev) => {
      let msg
      try {
        msg = JSON.parse(ev.data)
      } catch {
        return
      }
      const t = msg?.type
      if (t === 'offer') {
        try {
          await handleOffer(msg)
        } catch (e) {
          ElMessage.error(e.message || '处理 offer 失败')
        }
      } else if (t === 'ice' && pc) {
        try {
          if (msg.candidate?.candidate) await pc.addIceCandidate(msg.candidate)
        } catch (_) {
          /* ignore */
        }
      } else if (t === 'hangup') {
        ElMessage.info('对端已断开')
        cleanupWs()
        cleanupPeer()
        session.value = null
        expanded.value = false
        statusText.value = '对端断开'
        stopWaitTimer()
      }
    }
    ws.onclose = (ev) => {
      if (session.value) {
        statusText.value = `信令断开 code=${ev.code}`
      }
    }
    ws.onerror = () => {
      // 生产环境多为 IIS ARR 未开 WebSocket 反代；本机直连 8800 一般无此问题
      statusText.value = '信令 WebSocket 失败'
      ElMessage.error({
        duration: 12000,
        message:
          '信令 WebSocket 连接失败。生产站请在服务器：① 安装 IIS「WebSocket 协议」② ARR→Server Proxy Settings→勾选 Enable WebSocket proxy ③ iisreset；并确认 dist/web.config 已更新。',
      })
    }
  } catch (e) {
    session.value = null
    statusText.value = ''
    stopWaitTimer()
    ElMessage.error(e.message || '创建远控失败')
  } finally {
    starting.value = false
  }
}

function onGlobalKeyDown(e) {
  if (!session.value) return
  // 输入框内不抢快捷键
  const tag = (e.target?.tagName || '').toLowerCase()
  if (tag === 'input' || tag === 'textarea') return
  if (e.key === 'Escape') {
    e.preventDefault()
    handleEsc()
  } else if (e.key === 'F1') {
    e.preventDefault()
    helpVisible.value = true
  }
}

onMounted(async () => {
  document.addEventListener('fullscreenchange', onFullscreenChange)
  document.addEventListener('keydown', onGlobalKeyDown)
  await loadDevices()
  refreshTimer = setInterval(() => {
    if (!session.value) loadDevices()
  }, 15000)
})

onBeforeUnmount(async () => {
  document.removeEventListener('fullscreenchange', onFullscreenChange)
  document.removeEventListener('keydown', onGlobalKeyDown)
  if (document.fullscreenElement) {
    try {
      await document.exitFullscreen()
    } catch (_) {
      /* ignore */
    }
  }
  if (refreshTimer) {
    clearInterval(refreshTimer)
    refreshTimer = null
  }
  stopWaitTimer()
  if (session.value) {
    try {
      await api.stopRemoteSession(session.value.sessionId)
    } catch (_) {
      /* ignore */
    }
  }
  cleanupWs()
  cleanupPeer()
})
</script>

<style scoped>
.rd-page {
  display: flex;
  flex-direction: column;
  gap: 12px;
  height: calc(100vh - 140px);
  min-height: 520px;
}
.rd-page.is-expanded {
  height: calc(100vh - 100px);
}
.rd-tip {
  flex: 0 0 auto;
}
.toolbar {
  display: flex;
  flex-wrap: wrap;
  align-items: center;
  gap: 8px;
}
.status {
  color: #666;
  font-size: 13px;
}
.device-count {
  margin-left: auto;
  color: #888;
  font-size: 13px;
}
.opt-tag {
  margin-left: 8px;
}
.empty-tip {
  padding: 8px 12px;
  color: #999;
  font-size: 12px;
  line-height: 1.5;
}
.stage {
  position: relative;
  flex: 1;
  background: #111;
  border-radius: 8px;
  overflow: hidden;
  outline: none;
  min-height: 360px;
}
.stage.expanded {
  flex: 1 1 auto;
  min-height: calc(100vh - 160px);
  border-radius: 0;
}
.stage:fullscreen {
  width: 100vw;
  height: 100vh;
  border-radius: 0;
  background: #000;
}
.screen {
  width: 100%;
  height: 100%;
  object-fit: contain;
  background: #000;
  /* 减轻浏览器放大时的过度模糊 */
  image-rendering: -webkit-optimize-contrast;
}
.placeholder {
  position: absolute;
  inset: 0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #aaa;
  pointer-events: none;
  padding: 16px;
  text-align: center;
}
.stage-hint {
  position: absolute;
  right: 12px;
  bottom: 10px;
  color: rgba(255, 255, 255, 0.45);
  font-size: 12px;
  pointer-events: none;
  user-select: none;
}
.help-note {
  margin-top: 16px;
  color: #888;
  font-size: 13px;
  line-height: 1.6;
}
</style>
