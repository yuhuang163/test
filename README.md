# 路特产线管理平台

产测上位机云端后台 + 网页管理端，对应设计文档见 `D:\code\new_product_test\docs\云端工厂平台*.md`。

## 本地使用（只需记住两个 bat）

| 操作 | 双击 |
|------|------|
| **启动**前后端 | `D:\code\fwq\启动管理平台.bat` |
| **停止**前后端 | `D:\code\fwq\停止管理平台.bat` |

启动后会自动打开两个窗口（后端 + 管理网页）。浏览器访问：

- 管理网页：http://127.0.0.1:5173（请用 `127.0.0.1`，不要用 `localhost`，避免 IPv6 连不上）
- API 文档：http://127.0.0.1:8800/docs
- 默认账号：`admin` / `admin123`

若提示「拒绝连接」：看任务栏 **Lute-API** / **Lute-Admin** 窗口是否报错；首次启动需等 pip/npm 安装完成（1～3 分钟）。

首次运行会自动安装 Python 依赖（后端）和 npm 依赖（前端），稍等片刻即可。

改端口：编辑 `scripts/port.bat`，并同步 `factory-admin/vite.config.js` 里的 `API_PORT`。

## 目录
## 数据目录

- 本地开发（默认）：`factory-api/data/factory.db`、`factory-api/data/storage/...`
- 生产部署（建议）：默认指向 D 盘（`D:/fwq/data`），不会随代码包一起上传。请通过 `factory-api/.env` 中的 `DATABASE_URL` 与 `STORAGE_DIR` 覆盖默认路径。

**部署到服务器后请定期备份服务器上的数据目录（例如 `D:/fwq/data`）。**
├── 启动管理平台.bat      ← 本地启动入口
 将 `factory-api/`、`factory-admin/` 拷到服务器（或通过 git 拉取）。**不必上传** `.venv`、`node_modules`、`data/`。打包脚本会自动排除 `data/`，部署后服务器会使用 `factory-api/.env.production.example` 中的默认生产路径（指向 `D:/fwq/data`），如需更改请编辑 `factory-api/.env`。
├── factory-api/          # Python FastAPI 后端
├── factory-admin/        # Vue 3 管理网页
└── scripts/              # 内部脚本（一般不用手动点）
```

## 已实现功能

| 模块 | 状态 |
|------|------|
| 登录 / 日志上传与查询 | ✅ 完整实现（DB + 本地存储） |
| 账号管理 / 登录审计 / 改密 | ✅ 完整实现 |
| 元数据（工厂 / 工站 / 阈值键） | ✅ 工站与键为内置列表 |
| 阈值 / 用例 / OTA / 统一发布 | ⚠️ 示例实现（可联调，未持久化） |
| 设备登记 | ❌ 待实现 |

---

## API 接口

**BaseUrl**（本地）：`http://127.0.0.1:8800`  
**前缀**：`/api/factory-tool`  
**在线文档**：http://127.0.0.1:8800/docs

### 公共约定

**JSON 响应包络**（除文件下载外）：

```json
{ "code": 0, "message": "ok", "data": { } }
```

| code | 含义 |
|------|------|
| 0 | 成功 |
| 401 | 未登录 / Token 无效 |
| 403 | 无权限 |
| 404 | 资源不存在 |
| 409 | 版本冲突 |

**鉴权**：除标注「可匿名」外，请求头加 `Authorization: Bearer <accessToken>`。

**上位机建议请求头**：

```http
Device-Id: <电脑名>
Station-Key: <工站标识>
APP-Version: <exe 版本>
Authorization: Bearer <token>
```

---

### 认证 `/auth`

| 方法 | 路径 | 调用方 | 说明 |
|------|------|--------|------|
| POST | `/auth/login` | 网页、上位机 | body: `username`, `password`, `hostName`（必填）, `deviceId?`, `stationKey?` |
| GET | `/auth/me` | 网页 | 当前用户、角色、工站授权 |
| POST | `/auth/logout` | 网页、上位机 | 退出（当前为无状态 JWT） |
| POST | `/auth/change-password` | 网页 | body: `oldPassword`, `newPassword` |

登录成功 `data` 示例：

```json
{
  "accessToken": "...",
  "expireAt": "2026-06-17T03:00:00",
  "roles": ["admin"],
  "stationKeys": []
}
```

---

### 日志 `/logs`

| 方法 | 路径 | 调用方 | 说明 |
|------|------|--------|------|
| POST | `/logs/upload` | 上位机 | **可匿名**（`LOG_UPLOAD_ALLOW_ANONYMOUS=true` 时）；`multipart/form-data` |
| GET | `/logs` | 网页 | 分页列表，需登录 |
| GET | `/logs/{id}` | 网页 | 详情 + 解压文件树 |
| GET | `/logs/{id}/files/{path}` | 网页 | txt 在线预览，`text/plain` |
| GET | `/logs/{id}/download` | 网页 | 原 zip 下载，**二进制，无 JSON 包络** |

**上传字段**（`multipart/form-data`）：

| 字段 | 必填 | 说明 |
|------|------|------|
| `factoryName` | 是 | 工厂代码（来自上位机 `Mes/FACTORY`） |
| `deviceId` | 是 | 设备/电脑标识 |
| `station` | 是 | 工站名 |
| `file` | 是 | zip 包 |
| `sn` | 否 | 序列号 |
| `testResult` | 否 | 测试结果 |
| `clientVersion` | 否 | 上位机版本 |

**列表查询参数**：`factoryName`, `station`, `deviceId`, `hostName`, `sn`, `startTime`, `endTime`, `page`, `pageSize`

---

### 阈值 `/thresholds`、`/admin/threshold-templates`

| 方法 | 路径 | 调用方 | 说明 |
|------|------|--------|------|
| GET | `/thresholds` | 上位机 | query: `stationKey`, `productModel?`；返回 `version` + `items[]` |
| GET | `/admin/threshold-templates` | 网页 | 模板列表（engineer/admin） |
| GET | `/admin/threshold-templates/{id}` | 网页 | 模板详情 |
| POST | `/admin/threshold-templates` | 网页 | 新建草稿 |
| PUT | `/admin/threshold-templates/{id}` | 网页 | 编辑 |
| POST | `/admin/threshold-templates/{id}/publish` | 网页 | 发布新版本 |

`items[]` 元素：`{ "settingsKey": "BLE/LowRssi", "value": "-80" }`（键名须与上位机 `上位机设置.ini` 一致）

---

### 测试用例 `/test-cases`、`/admin/test-cases`

| 方法 | 路径 | 调用方 | 说明 |
|------|------|--------|------|
| GET | `/test-cases/manifest` | 上位机 | 待实现 |
| GET | `/test-cases/bundle` | 上位机 | zip 下载（用例包），待实现 |
| GET | `/admin/test-cases/files` | 网页 | 文件列表 / 树 |
| GET | `/admin/test-cases/files/{path}` | 网页 | 读取 ini 文本 |
| PUT | `/admin/test-cases/files/{path}` | 网页 | 保存 ini，`Content-Type: text/plain` |
| POST | `/admin/test-cases/publish` | 网页 | 发布用例包 |

---

### 上位机 OTA `/host-app`、`/admin/host-app`

| 方法 | 路径 | 调用方 | 说明 |
|------|------|--------|------|
| GET | `/host-app/check` | 上位机 | query: `packageName`, `buildId?`, `appVersion?`, `stationKey?`, `deviceId?` |
| GET | `/host-app/download/{buildId}` | 上位机 | exe 下载，待实现 |
| GET | `/admin/host-app/versions` | 网页 admin | 版本列表 |
| POST | `/admin/host-app/versions` | 网页 admin | 上传 exe（`multipart`）或登记元数据 |

---

### 统一发布 `/admin/releases`

| 方法 | 路径 | 调用方 | 说明 |
|------|------|--------|------|
| GET | `/admin/releases` | 网页 | 发布历史 |
| POST | `/admin/releases` | 网页 | 创建发布单 |
| GET | `/releases/check` | 上位机 | 一次拉取各层更新，待实现 |

---

### 元数据 `/admin/meta`

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/admin/meta/factories` | 工厂代码与中文名 |
| GET | `/admin/meta/stations` | 工站键与显示名 |
| GET | `/admin/meta/settings-keys` | 允许下发的 SETTINGS 键白名单 |

---

### 账号与审计 `/admin`

| 方法 | 路径 | 角色 | 说明 |
|------|------|------|------|
| GET | `/admin/users` | admin | 用户列表 |
| POST | `/admin/users` | admin | 创建用户 |
| PUT | `/admin/users/{id}` | admin | 改角色、工站、状态 |
| POST | `/admin/users/{id}/reset-password` | admin | 重置密码，返回新密码 |
| POST | `/admin/users/{id}/unlock` | admin | 解锁账号 |
| GET | `/admin/audit-logins` | admin | 登录审计；query: `username`, `hostName`, `page`, `pageSize` |
| GET | `/admin/devices` | admin | 设备登记列表，**待实现** |
| POST | `/admin/devices` | admin | 登记 PC，**待实现** |
| PUT | `/admin/devices/{id}` | admin | 编辑，**待实现** |
| DELETE | `/admin/devices/{id}` | admin | 删除，**待实现** |

**角色**：`operator`（日志只读）、`engineer`（阈值/用例/发布）、`admin`（全部）

---

### 健康检查

| 方法 | 路径 | 说明 |
|------|------|------|
| GET | `/health` | 无前缀；返回 `{ "status": "ok" }` |

---

## 上位机联调

设置页日志上传 URL：

```text
http://127.0.0.1:8800/api/factory-tool/logs/upload
```

上传前须在 MES 设置中选择工厂（`Mes/FACTORY` → `factoryName`）。

## 数据目录

- SQLite：`factory-api/data/factory.db`
- 日志文件：`factory-api/data/storage/logs/...`

**部署到服务器后请定期备份 `factory-api/data/` 整个目录。**

---

## 切换到服务器部署时要做什么

本地用 bat 是为了开发联调；上服务器后**不再用 bat**，改为「后端常驻进程 + Nginx 托管网页」。按下面清单逐项完成即可。

### 1. 准备服务器环境

- 操作系统：Linux（推荐 Ubuntu 22.04）或 Windows Server
- 安装 **Python 3.10+**
- 安装 **Nginx**（推荐，统一 80/443 入口）
- 前端只需在构建机或服务器上装 **Node.js**（`npm run build` 用，运行时不需要 node）

### 2. 上传代码

将 `factory-api/`、`factory-admin/` 拷到服务器（或通过 git 拉取）。**不必上传** `.venv`、`node_modules`、`data/`（生产数据在服务器上重新生成或从备份恢复）。

### 3. 配置后端

```bash
cd factory-api
python3 -m venv .venv
source .venv/bin/activate          # Windows: .\.venv\Scripts\activate
pip install -r requirements.txt
cp .env.example .env
```

编辑 `factory-api/.env`，**至少修改**：

| 配置项 | 说明 |
|--------|------|
| `SECRET_KEY` | 随机长字符串，勿用默认值 |
| `DEFAULT_ADMIN_PASSWORD` | 改掉默认 `admin123` |
| `CORS_ORIGINS` | 改成你的管理网页域名，如 `https://admin.example.com` |
| `LOG_UPLOAD_ALLOW_ANONYMOUS` | 过渡期可 `true`；正式环境建议 `false` 并给上位机加 Token |

用 **systemd** 或 **supervisor** 让后端开机自启（示例）：

```ini
# /etc/systemd/system/lute-factory-api.service
[Unit]
Description=路特产线管理平台 API
After=network.target

[Service]
WorkingDirectory=/opt/fwq/factory-api
ExecStart=/opt/fwq/factory-api/.venv/bin/uvicorn app.main:app --host 127.0.0.1 --port 8800
Restart=always

[Install]
WantedBy=multi-user.target
```

### 4. 构建并部署前端

```bash
cd factory-admin
npm install
npm run build
```

将生成的 `dist/` 目录放到 Nginx 可访问的位置（如 `/var/www/lute-factory-admin`）。

生产环境**不用 Vite 开发服务器**，`vite.config.js` 里的代理仅本地有效；线上由 Nginx 把 `/api` 转发到后端。

### 5. 配置 Nginx（示例）

```nginx
server {
    listen 443 ssl;
    server_name admin.example.com;

    # ssl_certificate ...（按实际证书配置）

    root /var/www/lute-factory-admin;
    index index.html;

    location /api/ {
        proxy_pass http://127.0.0.1:8800;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        client_max_body_size 500m;   # 日志 zip 上传大小
    }

    location / {
        try_files $uri $uri/ /index.html;
    }
}
```

若上位机从外网直传日志，可另开子域名或同域路径，确保 `POST /api/factory-tool/logs/upload` 可达且 body 大小足够。

### 6. 修改上位机配置

把所有指向本机的地址改为服务器域名，例如：

```text
https://admin.example.com/api/factory-tool/logs/upload
```

（路径前缀 `/api/factory-tool` 保持不变，只换域名和协议。）

### 7. 上线检查清单

- [ ] `.env` 中 `SECRET_KEY`、管理员密码已修改
- [ ] `factory-api/data/` 目录可写，已纳入备份计划
- [ ] 防火墙只开放 443（后端 8800 仅本机或内网访问）
- [ ] 浏览器能打开管理网页并登录
- [ ] 上位机试传一条日志，网页能查到
- [ ] HTTPS 证书有效（上位机若校验证书需用正规 CA）

### 本地 vs 服务器对照

| 项目 | 本地开发 | 服务器 |
|------|----------|--------|
| 启动方式 | `启动管理平台.bat` | systemd + Nginx |
| 管理网页 | Vite :5173 | Nginx 托管 `dist/` |
| API | uvicorn :8800 | uvicorn :8800（内网） |
| 前端访问 API | Vite 代理 `/api` | Nginx 反向代理 `/api` |
