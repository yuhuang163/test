/** 日期段工具：与 analytics API 的 startTime / endTime 对齐 */

function pad2(n) {
  return String(n).padStart(2, '0')
}

export function formatDateYmd(date) {
  return `${date.getFullYear()}-${pad2(date.getMonth() + 1)}-${pad2(date.getDate())}`
}

/** 默认最近 N 天（含今天），返回 [开始, 结束] YYYY-MM-DD */
export function defaultDateRange(days = 7) {
  const end = new Date()
  const start = new Date()
  start.setDate(start.getDate() - (days - 1))
  return [formatDateYmd(start), formatDateYmd(end)]
}

/** el-date-picker daterange → API 参数 */
export function toApiTimeRange(dateRange) {
  if (!dateRange || dateRange.length !== 2 || !dateRange[0] || !dateRange[1]) {
    return {}
  }
  return {
    startTime: `${dateRange[0]}T00:00:00`,
    endTime: `${dateRange[1]}T23:59:59`,
  }
}

export const DATE_RANGE_SHORTCUTS = [
  {
    text: '今天',
    value: () => {
      const d = formatDateYmd(new Date())
      return [d, d]
    },
  },
  {
    text: '最近7天',
    value: () => defaultDateRange(7),
  },
  {
    text: '最近30天',
    value: () => defaultDateRange(30),
  },
  {
    text: '本月',
    value: () => {
      const now = new Date()
      const start = new Date(now.getFullYear(), now.getMonth(), 1)
      return [formatDateYmd(start), formatDateYmd(now)]
    },
  },
]
