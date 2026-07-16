/** 日志预览：按 offset/limit 分页拉取，合并为完整文本。 */

const PREVIEW_CHUNK_SIZE = 200000

function authHeaders() {
  const token = localStorage.getItem('fc_token')
  return token ? { Authorization: `Bearer ${token}` } : {}
}

/**
 * 拉取日志包内单个文本文件的全文（服务端默认单次最多 20 万字符，此处自动分页）。
 * @param {number|string} logId
 * @param {string} relativePath zip 内相对路径
 * @returns {Promise<string>}
 */
export async function fetchLogPreviewText(logId, relativePath) {
  const encodedPath = encodeURIComponent(relativePath)
  const parts = []
  let offset = 0

  while (true) {
    const url = `/api/factory-tool/logs/${logId}/files/${encodedPath}?offset=${offset}&limit=${PREVIEW_CHUNK_SIZE}`
    const res = await fetch(url, { headers: authHeaders() })
    if (!res.ok) {
      throw new Error('预览失败')
    }

    const chunk = await res.text()
    parts.push(chunk)

    const hasMore = res.headers.get('X-Preview-Has-More') === 'true'
    if (!hasMore || !chunk.length) {
      break
    }

    const totalLength = Number.parseInt(res.headers.get('X-Preview-Total-Length') || '0', 10)
    offset += chunk.length
    if (totalLength > 0 && offset >= totalLength) {
      break
    }
  }

  return parts.join('')
}
