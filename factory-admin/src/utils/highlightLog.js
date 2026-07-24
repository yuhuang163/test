/**
 * 上位机日志预览高亮：转义 HTML 后按级别/关键词着色。
 * 规则与 Cursor 侧 logFileHighlighter.customPatterns 对齐。
 */

function escapeHtml(text) {
  return String(text)
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
}

/** 更长的中文词放前面，避免「不通过」被拆成「通过」 */
const TOKEN_RE =
  /(\[ERR(?:OR)?\]|\[WRN\]|\[WARN\]|\[DBG\]|\[DEBUG\]|\[UI\]|\d{4}-\d{2}-\d{2}[ T]\d{2}:\d{2}:\d{2}(?:\.\d+)?|卡控失败|卡控通过|不通过|测试完成|失败|通过|异常|超时|\b(?:ERROR|FAIL(?:URE)?|FATAL|WARNING|WARN)\b)/gi

function tokenClass(token) {
  const upper = token.toUpperCase()
  if (
    upper === '[ERR]' ||
    upper === '[ERROR]' ||
    upper === 'ERROR' ||
    upper === 'FAIL' ||
    upper === 'FAILURE' ||
    upper === 'FATAL'
  ) {
    return 'log-err'
  }
  if (upper === '[WRN]' || upper === '[WARN]' || upper === 'WARNING' || upper === 'WARN') {
    return 'log-wrn'
  }
  if (upper === '[DBG]' || upper === '[DEBUG]') {
    return 'log-dbg'
  }
  if (upper === '[UI]') {
    return 'log-ui'
  }
  if (/^\d{4}-\d{2}-\d{2}/.test(token)) {
    return 'log-time'
  }
  if (token === '卡控失败' || token === '不通过' || token === '失败' || token === '异常' || token === '超时') {
    return 'log-fail'
  }
  if (token === '卡控通过' || token === '通过' || token === '测试完成') {
    return 'log-pass'
  }
  return ''
}

export function highlightLogHtml(text) {
  if (text == null || text === '') {
    return ''
  }
  const escaped = escapeHtml(text)
  return escaped.replace(TOKEN_RE, (token) => {
    const cls = tokenClass(token)
    return cls ? `<span class="${cls}">${token}</span>` : token
  })
}
