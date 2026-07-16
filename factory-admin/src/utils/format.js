/** 将后端 UTC 时间（可能无 Z 后缀）解析为 Date */
export function parseApiDateTime(v) {
  if (!v) return null
  let s = String(v).trim()
  if (/^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}/.test(s) && !/[zZ]$|[+-]\d{2}:\d{2}$/.test(s)) {
  // 历史数据：naive ISO 按 UTC 理解
    s += 'Z'
  }
  const d = new Date(s)
  return Number.isNaN(d.getTime()) ? null : d
}

/** 显示为北京时间（与产线 PC 中国区时间一致） */
export function formatTime(v) {
  const d = parseApiDateTime(v)
  if (!d) return ''
  return d.toLocaleString('zh-CN', {
    hour12: false,
    timeZone: 'Asia/Shanghai',
  })
}

export function formatSize(n) {
  if (!n) return '0 B'
  if (n < 1024) return `${n} B`
  if (n < 1024 * 1024) return `${(n / 1024).toFixed(1)} KB`
  return `${(n / 1024 / 1024).toFixed(2)} MB`
}

/** 整机/分项结果是否为失败（NG、FAIL、失败等） */
export function isTestFailResult(v) {
  if (v == null || v === '') return false
  const s = String(v).trim()
  const u = s.toUpperCase()
  return u === 'NG' || u === 'FAIL' || s === '失败'
}

/** 整机/分项结果是否为通过（PASS、OK、通过等） */
export function isTestPassResult(v) {
  if (v == null || v === '') return false
  const s = String(v).trim()
  const u = s.toUpperCase()
  return u === 'PASS' || u === 'OK' || s === '通过'
}

/** 结果列样式：通过绿色、失败红色 */
export function testResultClass(v) {
  if (isTestFailResult(v)) return 'result-fail'
  if (isTestPassResult(v)) return 'result-pass'
  return ''
}
