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
      <el-select
        v-model="qualityPresetId"
        placeholder="画质"
        style="width: 168px"
        :disabled="!!session"
        @change="onQualityPresetChange"
      >
        <el-option
          v-for="p in qualityPresets"
          :key="p.id"
          :label="p.label"
          :value="p.id"
        />
      </el-select>
      <el-button type="danger" :disabled="!session && !selectedDeviceId" :loading="stopping" @click="confirmEndSession">
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
      <el-button :disabled="!session || !streamReady" @click="copyFromRemote">从远端复制</el-button>
      <el-button :disabled="!session || !streamReady" @click="pickFilesToRemote">发送文件</el-button>
      <input
        ref="fileInputRef"
        type="file"
        multiple
        style="display: none"
        @change="onFilesPicked"
      />
      <el-button :disabled="!session || !streamReady" @click="takeScreenshot">截图</el-button>
      <el-button @click="helpVisible = true">快捷键</el-button>

      <span v-if="statusText" class="status">{{ statusText }}</span>
      <el-tag v-if="session && streamReady" size="small" :type="latencyTagType" class="latency-tag" effect="plain">
        {{ latencyLabel }}
      </el-tag>
      <span class="device-count">在线 {{ devices.length }} 台</span>
    </div>

    <div
      ref="stageRef"
      class="stage"
      :class="{ expanded, 'has-stream': !!(session && streamReady) }"
      :style="stageCursorStyle"
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
      <div v-if="session && streamReady" class="stage-hud">
        <span>{{ latencyLabel }}</span>
        <span v-if="statsFps != null">{{ statsFps }} fps</span>
        <span v-if="statsKbps != null">{{ statsKbps }} kbps</span>
      </div>
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
        <el-descriptions-item label="画质">开始远控前选择；最高 1920@60。弱 CPU 软编可能自动降档，改画质需断开后重连</el-descriptions-item>
        <el-descriptions-item label="粘贴到远端">本机剪贴板文字/图片 → 产线机并 Ctrl+V</el-descriptions-item>
        <el-descriptions-item label="从远端复制">产线机文字/图片写入本机剪贴板；文件则下载到本机（画面内 Ctrl+C/X 也会自动回拉）</el-descriptions-item>
        <el-descriptions-item label="发送文件">选择本机文件传到产线机临时目录并放入远端剪贴板，可在资源管理器 Ctrl+V</el-descriptions-item>
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

/** 远控画质预设（创建会话时下发给产线 Agent；上限 1920@60） */
const RD_QUALITY_KEY = 'fc_rd_quality'
const qualityPresets = [
  { id: 'smooth', label: '流畅 1280@20', maxWidth: 1280, fps: 20, maxBitrate: 4_000_000 },
  { id: 'std', label: '标准 1280@30', maxWidth: 1280, fps: 30, maxBitrate: 6_000_000 },
  { id: 'hd', label: '高清 1920@30', maxWidth: 1920, fps: 30, maxBitrate: 12_000_000 },
  { id: 'ultra', label: '极致 1920@60', maxWidth: 1920, fps: 60, maxBitrate: 20_000_000 },
]
const qualityPresetId = ref('hd')

function loadQualityPreset() {
  try {
    const raw = localStorage.getItem(RD_QUALITY_KEY)
    if (!raw) return
    const saved = JSON.parse(raw)
    const id = saved?.id
    if (qualityPresets.some((p) => p.id === id)) qualityPresetId.value = id
  } catch (_) {
    /* ignore */
  }
}

function onQualityPresetChange() {
  const p = qualityPresets.find((x) => x.id === qualityPresetId.value) || qualityPresets[2]
  try {
    localStorage.setItem(RD_QUALITY_KEY, JSON.stringify({ id: p.id, ...p }))
  } catch (_) {
    /* ignore */
  }
}

function currentQuality() {
  return qualityPresets.find((x) => x.id === qualityPresetId.value) || qualityPresets[2]
}

/** 远端系统光标对应的 CSS cursor（全屏/窗口模式都用） */
const remoteCursor = ref('default')
/** DataChannel RTT（ms）；无通道时回退 ICE RTT */
const statsRttMs = ref(null)
const statsIceRttMs = ref(null)
const statsFps = ref(null)
const statsKbps = ref(null)

const latencyMs = computed(() => {
  if (statsRttMs.value != null) return statsRttMs.value
  return statsIceRttMs.value
})

const latencyLabel = computed(() => {
  const ms = latencyMs.value
  if (ms == null) return '延迟测量中…'
  return `延迟 ${ms} ms`
})

const latencyTagType = computed(() => {
  const ms = latencyMs.value
  if (ms == null) return 'info'
  if (ms <= 80) return 'success'
  if (ms <= 180) return 'warning'
  return 'danger'
})

const stageCursorStyle = computed(() => {
  if (!(session.value && streamReady.value)) return undefined
  return { cursor: remoteCursor.value || 'default' }
})

function applyRemoteCursor(css) {
  const next = css || 'default'
  remoteCursor.value = next
  const el = stageRef.value
  if (el) {
    // 全屏根节点用 important，避免浏览器/UA 样式盖住
    el.style.setProperty('cursor', next, 'important')
  }
}

const emptyHint = computed(() => {
  if (loadError.value) return loadError.value
  return '暂无在线心跳。请确认：① 上位机已登录；② BaseUrl 指向本管理端 API（本地一般为 http://127.0.0.1:8800）；③ 心跳间隔内保持运行'
})

const waitHint = computed(() => {
  const s = waitSeconds.value
  if (s < 3) return `正在通知产线拉起 Agent…（${s}s）`
  if (s < 10) return `等待 Agent 推流…（${s}s，命令轮询约 0.8 秒）`
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
let statsTimer = null
let lastBytesReceived = 0
let lastBytesAt = 0
let pendingPingAt = 0
let lastRecvDiagAt = 0
let inboundCodecId = null

function resetStats() {
  statsRttMs.value = null
  statsIceRttMs.value = null
  statsFps.value = null
  statsKbps.value = null
  lastBytesReceived = 0
  lastBytesAt = 0
  pendingPingAt = 0
  lastRecvDiagAt = 0
  inboundCodecId = null
}

function stopStatsTimer() {
  if (statsTimer) {
    clearInterval(statsTimer)
    statsTimer = null
  }
}

function startStatsTimer() {
  stopStatsTimer()
  resetStats()
  statsTimer = setInterval(() => {
    pollPeerStats()
    sendLatencyPing()
  }, 1000)
}

function sendLatencyPing() {
  if (!inputChannel || inputChannel.readyState !== 'open') return
  try {
    pendingPingAt = performance.now()
    inputChannel.send(JSON.stringify({ type: 'ping', t: Date.now() }))
  } catch (_) {
    /* ignore */
  }
}

function onDataChannelMessage(raw) {
  try {
    const text = typeof raw === 'string' ? raw : new TextDecoder().decode(raw)
    const msg = JSON.parse(text)
    if (msg?.type === 'pong' && pendingPingAt > 0) {
      statsRttMs.value = Math.max(0, Math.round(performance.now() - pendingPingAt))
      pendingPingAt = 0
    } else if (msg?.type === 'cursor') {
      applyRemoteCursor(msg.cursor)
    } else if (msg?.type === 'clipboard') {
      handleClipboardMeta(msg)
    } else if (msg?.type === 'clipboard_part' && msg?.dir !== 'to_agent') {
      handleClipboardPart(msg)
    } else if (msg?.type === 'paste_done') {
      if (msg.ok === false) {
        ElMessage.error(msg.error || '粘贴到远端失败')
      } else if (msg.kind === 'files') {
        ElMessage.success(`已发送 ${msg.count || 0} 个文件到远端剪贴板，请在资源管理器 Ctrl+V`)
      }
    }
  } catch (_) {
    /* ignore */
  }
}

async function pollPeerStats() {
  if (!pc) return
  try {
    const report = await pc.getStats()
    let iceRtt = null
    let fps = null
    let bytes = null
    let frameW = null
    let frameH = null
    let framesDecoded = null
    let framesDropped = null
    let freezeCount = null
    let jitter = null
    let packetsLost = null
    let packetsReceived = null
    let mime = null
    let decoderImpl = null
    report.forEach((s) => {
      if (s.type === 'candidate-pair' && (s.state === 'succeeded' || s.nominated)) {
        if (typeof s.currentRoundTripTime === 'number') {
          iceRtt = Math.round(s.currentRoundTripTime * 1000)
        }
      }
      if (s.type === 'inbound-rtp' && s.kind === 'video') {
        if (typeof s.framesPerSecond === 'number') fps = Math.round(s.framesPerSecond)
        if (typeof s.bytesReceived === 'number') bytes = s.bytesReceived
        if (typeof s.frameWidth === 'number') frameW = s.frameWidth
        if (typeof s.frameHeight === 'number') frameH = s.frameHeight
        if (typeof s.framesDecoded === 'number') framesDecoded = s.framesDecoded
        if (typeof s.framesDropped === 'number') framesDropped = s.framesDropped
        if (typeof s.freezeCount === 'number') freezeCount = s.freezeCount
        if (typeof s.jitter === 'number') jitter = s.jitter
        if (typeof s.packetsLost === 'number') packetsLost = s.packetsLost
        if (typeof s.packetsReceived === 'number') packetsReceived = s.packetsReceived
        if (s.codecId) inboundCodecId = s.codecId
      }
      if (s.type === 'codec' && inboundCodecId && s.id === inboundCodecId) {
        mime = s.mimeType || null
      }
      if (s.type === 'track' && s.kind === 'video') {
        if (typeof s.frameWidth === 'number' && frameW == null) frameW = s.frameWidth
        if (typeof s.frameHeight === 'number' && frameH == null) frameH = s.frameHeight
      }
      if (s.type === 'media-source' && s.kind === 'video') {
        /* ignore */
      }
    })
    // 二次遍历拿 codec（codec 可能先于 inbound-rtp）
    if (inboundCodecId && !mime) {
      report.forEach((s) => {
        if (s.type === 'codec' && s.id === inboundCodecId) mime = s.mimeType || null
      })
    }
    try {
      const v = videoRef.value
      if (v && typeof v.getVideoPlaybackQuality === 'function') {
        const q = v.getVideoPlaybackQuality()
        if (q && typeof q.droppedVideoFrames === 'number' && framesDropped == null) {
          framesDropped = q.droppedVideoFrames
        }
      }
      decoderImpl = videoRef.value?.videoWidth
        ? `${videoRef.value.videoWidth}x${videoRef.value.videoHeight}`
        : null
    } catch (_) {
      /* ignore */
    }
    if (iceRtt != null) statsIceRttMs.value = iceRtt
    if (fps != null) statsFps.value = fps
    const now = performance.now()
    let kbps = null
    if (bytes != null) {
      if (lastBytesAt > 0) {
        const dt = (now - lastBytesAt) / 1000
        if (dt > 0.2) {
          kbps = Math.round(((bytes - lastBytesReceived) * 8) / dt / 1000)
          statsKbps.value = Math.max(0, kbps)
        }
      }
      lastBytesReceived = bytes
      lastBytesAt = now
    }
    // 每 ~5s 打一条接收侧诊断，对照 Agent [stream] 日志排查糊屏
    if (now - lastRecvDiagAt >= 5000) {
      lastRecvDiagAt = now
      const lossPct =
        packetsReceived != null && packetsLost != null && packetsReceived + packetsLost > 0
          ? (((packetsLost) / (packetsReceived + packetsLost)) * 100).toFixed(2)
          : '?'
      console.info(
        '[rd-recv]',
        `size=${frameW || '?'}x${frameH || '?'}`,
        `videoEl=${decoderImpl || '?'}`,
        `fps=${fps ?? '?'}`,
        `kbps=${kbps ?? statsKbps.value ?? '?'}`,
        `mime=${mime || '?'}`,
        `dropped=${framesDropped ?? '?'}`,
        `freeze=${freezeCount ?? '?'}`,
        `decoded=${framesDecoded ?? '?'}`,
        `loss%=${lossPct}`,
        `jitter=${jitter != null ? jitter.toFixed(4) : '?'}`,
        `rtt=${statsRttMs.value ?? iceRtt ?? '?'}ms`,
        `scale=${scaleMode.value}`,
        `screenMeta=${screenWidth}x${screenHeight}`
      )
      if (frameW && screenWidth && frameW < screenWidth * 0.75) {
        console.warn(
          '[rd-recv] 解码分辨率明显低于远端屏 meta，画面会被放大变糊',
          `${frameW}x${frameH}`,
          'vs',
          `${screenWidth}x${screenHeight}`
        )
      }
      if ((kbps ?? statsKbps.value ?? 0) > 0 && (kbps ?? statsKbps.value) < 1500 && (frameW || 0) >= 1280) {
        console.warn('[rd-recv] 接收码率偏低，易出现块效应/发糊', `kbps=${kbps ?? statsKbps.value}`)
      }
    }
  } catch (_) {
    /* ignore */
  }
}

function preferLowLatencyPlayback() {
  if (!pc) return
  try {
    for (const receiver of pc.getReceivers()) {
      if (receiver.track?.kind !== 'video') continue
      try {
        receiver.playoutDelayHint = 0
      } catch (_) {
        /* ignore */
      }
      try {
        receiver.jitterBufferTarget = 0
      } catch (_) {
        /* ignore */
      }
    }
  } catch (_) {
    /* ignore */
  }
  if (videoRef.value) {
    try {
      videoRef.value.playsInline = true
      // 部分浏览器支持降低缓冲
      if ('latencyHint' in videoRef.value) videoRef.value.latencyHint = 'interactive'
    } catch (_) {
      /* ignore */
    }
  }
}

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
  // 进入/退出全屏后重新施加远端光标
  if (session.value && streamReady.value) {
    applyRemoteCursor(remoteCursor.value)
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
  // 刷新后 session 丢失时，仍可按所选设备强制断开残留会话
  if (!session.value && !selectedDeviceId.value) return
  if (session.value && streamReady.value) {
    try {
      await ElMessageBox.confirm('确定断开远控？', '退出远控', {
        type: 'warning',
        confirmButtonText: '断开',
        cancelButtonText: '取消',
      })
    } catch {
      return
    }
  } else if (!session.value && selectedDeviceId.value) {
    try {
      await ElMessageBox.confirm(
        '当前页没有进行中的画面会话。是否强制清除该设备在服务端残留的远控锁？',
        '强制断开',
        { type: 'warning', confirmButtonText: '强制断开', cancelButtonText: '取消' }
      )
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
  stopStatsTimer()
  resetStats()
  applyRemoteCursor('default')
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
  stopping.value = true
  const sid = session.value?.sessionId
  const deviceId = session.value?.deviceId || selectedDeviceId.value
  try {
    try {
      ws?.send(JSON.stringify({ type: 'hangup', reason: 'viewer_hangup' }))
    } catch (_) {
      /* ignore */
    }
    if (sid) {
      await api.stopRemoteSession(sid)
    } else if (deviceId) {
      await api.stopRemoteSessionByDevice(deviceId)
    }
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
    api.clearRememberedSession()
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

const CLIP_CHUNK = 24 * 1024
const fileInputRef = ref(null)

async function pasteClipboard() {
  try {
    if (!inputChannel || inputChannel.readyState !== 'open') {
      ElMessage.warning('键鼠通道未就绪')
      return
    }
    // 优先 ClipboardItem（文字+图片）
    if (navigator.clipboard?.read) {
      try {
        const items = await navigator.clipboard.read()
        for (const item of items) {
          if (item.types.includes('image/png')) {
            const blob = await item.getType('image/png')
            await sendImageToRemote(blob)
            ElMessage.success('已粘贴图片到远端')
            stageRef.value?.focus()
            return
          }
          if (item.types.includes('text/plain')) {
            const blob = await item.getType('text/plain')
            const text = await blob.text()
            if (text) {
              const clipped = text.length > 200000 ? text.slice(0, 200000) : text
              sendInput({ type: 'paste', kind: 'text', text: clipped, reqId: ++clipboardReqSeq })
              ElMessage.success('已粘贴文字到远端')
              stageRef.value?.focus()
              return
            }
          }
        }
      } catch (_) {
        /* fallback readText */
      }
    }
    const text = await navigator.clipboard.readText()
    if (!text) {
      ElMessage.warning('本机剪贴板没有可粘贴的文字/图片（文件请用「发送文件」）')
      return
    }
    const clipped = text.length > 200000 ? text.slice(0, 200000) : text
    sendInput({ type: 'paste', kind: 'text', text: clipped, reqId: ++clipboardReqSeq })
    ElMessage.success('已粘贴到远端')
    stageRef.value?.focus()
  } catch (e) {
    ElMessage.error(e.message || '读取剪贴板失败（需 HTTPS 或本机授权）')
  }
}

function pickFilesToRemote() {
  fileInputRef.value?.click()
}

async function onFilesPicked(e) {
  const files = Array.from(e.target?.files || [])
  e.target.value = ''
  if (!files.length) return
  try {
    await sendFilesToRemote(files)
  } catch (err) {
    ElMessage.error(err.message || '发送文件失败')
  }
}

function u8ToBase64(u8) {
  let s = ''
  const chunk = 0x8000
  for (let i = 0; i < u8.length; i += chunk) {
    s += String.fromCharCode.apply(null, u8.subarray(i, i + chunk))
  }
  return btoa(s)
}

function chunkBytes(u8, size = CLIP_CHUNK) {
  const parts = []
  for (let i = 0; i < u8.length; i += size) {
    parts.push(u8ToBase64(u8.subarray(i, i + size)))
  }
  return parts.length ? parts : ['']
}

async function sendImageToRemote(blob) {
  const buf = new Uint8Array(await blob.arrayBuffer())
  if (buf.length > 6 * 1024 * 1024) throw new Error('图片过大（>6MB）')
  const reqId = ++clipboardReqSeq
  const parts = chunkBytes(buf)
  sendInput({
    type: 'paste',
    kind: 'image',
    mime: 'image/png',
    parts: parts.length,
    bytes: buf.length,
    reqId,
  })
  for (let i = 0; i < parts.length; i++) {
    sendInput({
      type: 'clipboard_part',
      dir: 'to_agent',
      role: 'image',
      reqId,
      index: i,
      total: parts.length,
      data: parts[i],
    })
  }
}

async function sendFilesToRemote(fileList) {
  if (!inputChannel || inputChannel.readyState !== 'open') {
    throw new Error('键鼠通道未就绪')
  }
  const files = fileList.slice(0, 10)
  const packed = []
  let total = 0
  for (const f of files) {
    if (f.size > 12 * 1024 * 1024) {
      ElMessage.warning(`跳过过大文件：${f.name}`)
      continue
    }
    if (total + f.size > 24 * 1024 * 1024) {
      ElMessage.warning('已达总大小上限，其余文件跳过')
      break
    }
    const buf = new Uint8Array(await f.arrayBuffer())
    total += buf.length
    packed.push({ name: f.name, buf })
  }
  if (!packed.length) throw new Error('没有可发送的文件')
  const reqId = ++clipboardReqSeq
  const meta = packed.map((p, fileIndex) => {
    const parts = chunkBytes(p.buf)
    return { name: p.name, bytes: p.buf.length, parts: parts.length, fileIndex, _parts: parts }
  })
  sendInput({
    type: 'paste',
    kind: 'files',
    reqId,
    fileCount: meta.length,
    files: meta.map(({ name, bytes, parts, fileIndex }) => ({ name, bytes, parts, fileIndex })),
  })
  for (const m of meta) {
    for (let i = 0; i < m._parts.length; i++) {
      sendInput({
        type: 'clipboard_part',
        dir: 'to_agent',
        role: 'file',
        reqId,
        name: m.name,
        fileIndex: m.fileIndex,
        index: i,
        total: m._parts.length,
        data: m._parts[i],
      })
    }
  }
  ElMessage.info(`正在发送 ${meta.length} 个文件…`)
}

/** 等待 Agent 回传 clipboard 的 Promise 队列 */
const clipboardWaiters = []
const clipboardAssemblers = new Map()
let clipboardPullTimer = null
let clipboardReqSeq = 0

function resolveClipboardWaiters(reqId, payload) {
  for (let i = clipboardWaiters.length - 1; i >= 0; i--) {
    const w = clipboardWaiters[i]
    if (reqId != null && w.reqId !== reqId && clipboardWaiters.length > 1) continue
    clipboardWaiters.splice(i, 1)
    try {
      w.resolve(payload)
    } catch (_) {
      /* ignore */
    }
    if (reqId != null) break
  }
}

function handleClipboardMeta(msg) {
  const reqId = msg.reqId
  const kind = msg.kind || (msg.text != null ? 'text' : 'empty')
  const err = msg.error ? String(msg.error) : ''
  if (err) {
    resolveClipboardWaiters(reqId, { kind: 'empty', error: err })
    clipboardAssemblers.delete(reqId)
    return
  }
  if (kind === 'text' || kind === 'empty') {
    resolveClipboardWaiters(reqId, { kind, text: String(msg.text || ''), error: '' })
    return
  }
  if (kind === 'image') {
    clipboardAssemblers.set(reqId, {
      kind: 'image',
      mime: msg.mime || 'image/png',
      total: Number(msg.parts || 0),
      parts: Array(Number(msg.parts || 0)).fill(null),
      got: 0,
    })
    return
  }
  if (kind === 'files') {
    const files = {}
    for (const f of msg.files || []) {
      const fi = Number(f.fileIndex ?? 0)
      files[fi] = {
        name: f.name || `file${fi}`,
        error: f.error || '',
        total: Number(f.parts || 0),
        parts: f.error ? [] : Array(Number(f.parts || 0)).fill(null),
        got: 0,
        bytes: Number(f.bytes || 0),
      }
    }
    clipboardAssemblers.set(reqId, { kind: 'files', files })
    // 若全是 error、无分片，直接完成
    const pending = Object.values(files).some((f) => !f.error && f.total > 0)
    if (!pending) {
      finishAssembledClipboard(reqId)
    }
  }
}

function b64ToU8(b64) {
  const bin = atob(b64 || '')
  const u8 = new Uint8Array(bin.length)
  for (let i = 0; i < bin.length; i++) u8[i] = bin.charCodeAt(i)
  return u8
}

function concatU8(parts) {
  const total = parts.reduce((n, p) => n + p.length, 0)
  const out = new Uint8Array(total)
  let off = 0
  for (const p of parts) {
    out.set(p, off)
    off += p.length
  }
  return out
}

function handleClipboardPart(msg) {
  const reqId = msg.reqId
  const slot = clipboardAssemblers.get(reqId)
  if (!slot) return
  const idx = Number(msg.index || 0)
  const raw = b64ToU8(msg.data)
  if (msg.role === 'image' && slot.kind === 'image') {
    if (idx >= 0 && idx < slot.parts.length && slot.parts[idx] == null) {
      slot.parts[idx] = raw
      slot.got += 1
    }
    if (slot.got >= slot.total && slot.parts.every((p) => p != null)) {
      finishAssembledClipboard(reqId)
    }
    return
  }
  if (msg.role === 'file' && slot.kind === 'files') {
    const fi = Number(msg.fileIndex || 0)
    const info = slot.files[fi]
    if (!info || info.error) return
    if (idx >= 0 && idx < info.parts.length && info.parts[idx] == null) {
      info.parts[idx] = raw
      info.got += 1
    }
    const done = Object.values(slot.files).every(
      (f) => f.error || (f.got >= f.total && f.parts.every((p) => p != null)),
    )
    if (done) finishAssembledClipboard(reqId)
  }
}

function finishAssembledClipboard(reqId) {
  const slot = clipboardAssemblers.get(reqId)
  clipboardAssemblers.delete(reqId)
  if (!slot) return
  if (slot.kind === 'image') {
    const u8 = concatU8(slot.parts)
    resolveClipboardWaiters(reqId, {
      kind: 'image',
      mime: slot.mime || 'image/png',
      bytes: u8,
      error: '',
    })
    return
  }
  if (slot.kind === 'files') {
    const files = []
    for (const fi of Object.keys(slot.files)
      .map(Number)
      .sort((a, b) => a - b)) {
      const f = slot.files[fi]
      if (f.error) {
        files.push({ name: f.name, error: f.error })
      } else {
        files.push({ name: f.name, bytes: concatU8(f.parts) })
      }
    }
    resolveClipboardWaiters(reqId, { kind: 'files', files, error: '' })
  }
}

function requestRemoteClipboard(timeoutMs = 15000) {
  return new Promise((resolve, reject) => {
    if (!inputChannel || inputChannel.readyState !== 'open') {
      reject(new Error('键鼠通道未就绪，无法读取远端剪贴板'))
      return
    }
    const reqId = ++clipboardReqSeq
    const timer = setTimeout(() => {
      const idx = clipboardWaiters.findIndex((w) => w.reqId === reqId)
      if (idx >= 0) clipboardWaiters.splice(idx, 1)
      clipboardAssemblers.delete(reqId)
      reject(new Error('读取远端剪贴板超时'))
    }, timeoutMs)
    clipboardWaiters.push({
      reqId,
      resolve: (payload) => {
        clearTimeout(timer)
        resolve(payload)
      },
    })
    sendInput({ type: 'clipboard_get', reqId })
  })
}

function downloadBytes(name, u8) {
  const blob = new Blob([u8])
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = name || 'file.bin'
  a.click()
  URL.revokeObjectURL(url)
}

async function applyRemoteClipboardToLocal(payload, { silent = false } = {}) {
  if (!payload || payload.error) {
    if (!silent) ElMessage.warning(payload?.error || '远端剪贴板为空')
    return false
  }
  const kind = payload.kind || 'empty'
  try {
    if (kind === 'text') {
      const clip = String(payload.text || '')
      if (!clip) {
        if (!silent) ElMessage.warning('远端剪贴板没有文字')
        return false
      }
      await navigator.clipboard.writeText(clip)
      if (!silent) ElMessage.success(`已复制远端文字（${clip.length} 字）`)
      return true
    }
    if (kind === 'image') {
      const u8 = payload.bytes
      const blob = new Blob([u8], { type: payload.mime || 'image/png' })
      if (navigator.clipboard?.write && window.ClipboardItem) {
        await navigator.clipboard.write([new ClipboardItem({ [blob.type]: blob })])
        if (!silent) ElMessage.success('已复制远端图片到本机剪贴板')
      } else {
        downloadBytes(`remote_clip_${Date.now()}.png`, u8)
        if (!silent) ElMessage.success('浏览器不支持写图片剪贴板，已下载 PNG')
      }
      return true
    }
    if (kind === 'files') {
      const files = payload.files || []
      let ok = 0
      let fail = 0
      for (const f of files) {
        if (f.error || !f.bytes) {
          fail += 1
          continue
        }
        downloadBytes(f.name, f.bytes)
        ok += 1
      }
      if (!silent) {
        if (ok) ElMessage.success(`已下载远端 ${ok} 个文件${fail ? `（${fail} 个失败/跳过）` : ''}`)
        else ElMessage.warning('远端文件无法传输（可能过大或为文件夹）')
      }
      return ok > 0
    }
    if (!silent) ElMessage.warning('远端剪贴板为空')
    return false
  } catch (e) {
    if (!silent) ElMessage.error(e.message || '写入本机失败（需 HTTPS 或授权）')
    return false
  }
}

async function copyFromRemote() {
  try {
    const payload = await requestRemoteClipboard()
    await applyRemoteClipboardToLocal(payload, { silent: false })
    stageRef.value?.focus()
  } catch (e) {
    ElMessage.error(e.message || '读取远端剪贴板失败')
  }
}

/** 远控画面内 Ctrl/Cmd+C、X 后，延迟回拉产线机剪贴板到本机 */
function schedulePullRemoteClipboard() {
  if (clipboardPullTimer) clearTimeout(clipboardPullTimer)
  clipboardPullTimer = setTimeout(async () => {
    clipboardPullTimer = null
    try {
      const payload = await requestRemoteClipboard(12000)
      if (!payload || payload.error || payload.kind === 'empty') return
      if (payload.kind === 'text' && !payload.text) return
      await applyRemoteClipboardToLocal(payload, { silent: true })
    } catch (_) {
      /* 静默 */
    }
  }, 280)
}

function isCopyOrCutShortcut(e) {
  if (!(e.ctrlKey || e.metaKey) || e.altKey || e.shiftKey) return false
  const k = String(e.key || '').toLowerCase()
  if (k === 'c' || k === 'x') return true
  const code = e.keyCode || e.which
  return code === 67 || code === 88
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
  // 键鼠尽快发，略提高采样
  if (now - lastMoveSent < 12) return
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
  // 远端复制/剪切后回拉剪贴板到本机
  if (isCopyOrCutShortcut(e)) {
    schedulePullRemoteClipboard()
  }
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
    sendLatencyPing()
  }
  ch.onmessage = (ev) => onDataChannelMessage(ev.data)
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
      preferLowLatencyPlayback()
      startStatsTimer()
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
  // 优先 H264（Windows 浏览器硬解更稳、延迟通常更好）
  try {
    const caps = RTCRtpReceiver.getCapabilities?.('video')
    if (caps?.codecs?.length) {
      const preferred = [...caps.codecs].sort((a, b) => {
        const rank = (c) => ((c.mimeType || '').toLowerCase().includes('h264') ? 0 : 1)
        return rank(a) - rank(b)
      })
      for (const t of pc.getTransceivers()) {
        if (t.receiver?.track?.kind === 'video' || t.mid != null) {
          try {
            t.setCodecPreferences(preferred)
          } catch (_) {
            /* ignore */
          }
        }
      }
    }
  } catch (_) {
    /* ignore */
  }
  const answer = await pc.createAnswer()
  await pc.setLocalDescription(answer)
  preferLowLatencyPlayback()
  ws.send(JSON.stringify({ type: 'answer', sdp: pc.localDescription.sdp }))
}

async function startSession() {
  if (!selectedDeviceId.value) return
  starting.value = true
  statusText.value = '创建会话…'
  try {
    // force=true：刷新后服务端残留锁可直接顶替
    const q = currentQuality()
    const data = await api.createRemoteSession(selectedDeviceId.value, {
      force: true,
      maxWidth: q.maxWidth,
      fps: q.fps,
      maxBitrate: q.maxBitrate,
    })
    session.value = data
    api.rememberSession(data)
    streamReady.value = false
    statusText.value = `等待 Agent…（${q.label}）`
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
        api.clearRememberedSession()
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
        duration: 14000,
        message:
          '信令 WebSocket 连接失败（常见 code=1006）。生产站请确认：① 已安装 IIS「WebSocket 协议」② ARR→Server Proxy Settings→勾选 Enable proxy（ARR3 通常无单独 WebSocket proxy 勾选项）③ 站点 URL 重写已允许变量 HTTP_SEC_WEBSOCKET_EXTENSIONS，且 dist/web.config 的 API Proxy 已清空该头 ④ iisreset。服务器本机可对比测 ws://127.0.0.1:8800/... 与 ws://127.0.0.1/api/...',
      })
    }
  } catch (e) {
    session.value = null
    api.clearRememberedSession()
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

function onPageHide() {
  // 刷新/关页时 await 常来不及；用 keepalive 尽量释放服务端设备锁
  const sid = session.value?.sessionId || api.readRememberedSession()?.sessionId
  if (sid) api.beaconStopSession(sid)
}

onMounted(async () => {
  document.addEventListener('fullscreenchange', onFullscreenChange)
  document.addEventListener('keydown', onGlobalKeyDown)
  window.addEventListener('pagehide', onPageHide)
  loadQualityPreset()
  onQualityPresetChange()
  await loadDevices()
  // 若上次刷新未清掉锁，选中设备后可直接「断开」或再点「开始远控」顶替
  const remembered = api.readRememberedSession()
  if (remembered?.deviceId && !selectedDeviceId.value) {
    selectedDeviceId.value = remembered.deviceId
  }
  refreshTimer = setInterval(() => {
    if (!session.value) loadDevices()
  }, 15000)
})

onBeforeUnmount(async () => {
  document.removeEventListener('fullscreenchange', onFullscreenChange)
  document.removeEventListener('keydown', onGlobalKeyDown)
  window.removeEventListener('pagehide', onPageHide)
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
  stopStatsTimer()
  const sid = session.value?.sessionId
  if (sid) {
    api.beaconStopSession(sid)
    try {
      await api.stopRemoteSession(sid)
    } catch (_) {
      /* ignore */
    }
  }
  api.clearRememberedSession()
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
.latency-tag {
  margin-left: 4px;
}
.device-count {
  margin-left: auto;
  color: #888;
  font-size: 13px;
}
.stage-hud {
  position: absolute;
  left: 12px;
  top: 10px;
  display: flex;
  gap: 10px;
  color: rgba(255, 255, 255, 0.85);
  font-size: 12px;
  font-variant-numeric: tabular-nums;
  background: rgba(0, 0, 0, 0.45);
  padding: 4px 10px;
  border-radius: 6px;
  pointer-events: none;
  user-select: none;
  z-index: 2;
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
.stage.has-stream,
.stage.has-stream .screen {
  /* 实际 cursor 由 JS setProperty(..., important) 控制 */
  cursor: inherit;
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
