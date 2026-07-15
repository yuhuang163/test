# fwq 通信协议 — 路特产线管理平台 REST API

> 本文描述产测**上位机**与**路特产线管理平台**（仓库 `fwq`，生产域名 `fctp.luteos.com`）之间的 **HTTPS REST** 通信格式。  
> 服务端实现：`D:\code\fwq\factory-api\`。  
> 上位机客户端：`platform/cloud/client/factory_cloud_client.cpp` 及各 `*Service`。  
> 在线 OpenAPI：`{BaseUrl}/docs`（本地开发 `http://127.0.0.1:8800/docs`）。

---

## 1. 通信约定

### 1.1 连接与路径前缀

| 项目 | 说明 |
|------|------|
| 协议 | **HTTPS**（生产）/ **HTTP**（本地联调） |
| 默认生产地址 | `https://fctp.luteos.com` |
| 本地 API | `http://127.0.0.1:8800`（运行 `fwq/启动管理平台.bat`） |
| API 前缀 | `/api/factory-tool` |
| 完整 API 根 | `{BaseUrl}/api/factory-tool` |
| 健康检查 | `GET {BaseUrl}/health` → `{"status":"ok"}`（**无前缀**，非 JSON 包络） |
| 字符编码 | 请求/响应 JSON 均为 **UTF-8** |
| 超时 | 上位机默认 **120 秒** |

上位机 `上位机设置.ini` / `上位机设置.local.ini` 键 **`FactoryCloud/BaseUrl`** 填站点根地址（不含 `/api/factory-tool`）。  
工厂代码上报字段 **`factoryName`** 与 **`Mes/FACTORY`** 同源。

**IIS 反代**（生产）：`/api/*`、`/docs`、`/openapi.json`、`/health` 转发至本机 `127.0.0.1:8800`，见 `fwq/factory-admin/public/web.config`。

---

### 1.2 JSON 响应包络

除**文件下载**外，成功与业务失败均返回 JSON，结构固定为三字段：

| 字段 | 类型 | 说明 |
|------|------|------|
| 业务码 | 整数 | `0` 表示成功；非 `0` 见 §1.5 |
| 消息 | 字符串 | 人类可读说明，成功时多为 `ok` |
| 数据 | 对象 / 数组 / 空 | 成功时的业务载荷；失败时多为 `null` |

示例（成功）：

```json
{
  "code": 0,
  "message": "ok",
  "data": { }
}
```

**文件类响应**（zip / exe / octet-stream）直接返回字节流，**无上述包络**；HTTP 状态码 `200` 即表示下载成功。

---

### 1.3 请求头

#### 1.3.1 通用

| 字段 | 必填 | 说明 |
|------|------|------|
| 内容类型 | JSON 请求时必填 | `application/json` |
| 授权 | 除匿名接口外必填 | `Bearer <accessToken>`，Token 来自登录 |

#### 1.3.2 上位机建议携带（`FactoryCloudClient::applyFactoryHeaders`）

| 字段 | 必填 | 说明 |
|------|------|------|
| 设备标识 | 建议 | 默认电脑名；可配置 `FactoryCloud/DeviceId` |
| 工站键 | 建议 | 当前工站名，与 `TestOrderMeta/SelectedStationName` 同源 |
| 应用版本 | 建议 | 从 `DEBUG_VER` 宏解析，如 `1.6.2` |
| 构建号 | 可选 | exe 名 `new_production_YYYYMMDD` 中的日期 |
| 授权 | 登录后 | `Bearer` + `FactoryCloud/Token` |

网页管理端（`factory-admin`）一般仅带 **授权** 头。

---

### 1.4 鉴权

| 项目 | 说明 |
|------|------|
| 方式 | **JWT**（HS256），无状态；`POST /auth/logout` 不吊销 Token |
| 有效期 | 默认 **12 小时**（环境变量 `ACCESS_TOKEN_EXPIRE_HOURS`） |
| 角色 | `operator` / `engineer` / `admin`（admin 可访问全部） |
| 工站授权 | 用户绑定 `stationKeys`；登录时若传 `stationKey` 会校验（含 PCBA↔PCBA_TEST 等别名） |
| 工厂隔离 | 按用户 `factoryCode` 过滤列表与详情 |
| 登录锁定 | 连续 **5 次**密码错误锁定 **30 分钟** |

**匿名允许**（可配置）：`POST /logs/upload` 在 `LOG_UPLOAD_ALLOW_ANONYMOUS=true` 时可不带 Token；`POST /test-records` 当前实现**不强制**登录。

---

### 1.5 错误码

| 业务码 | HTTP | 说明 | 典型场景 |
|--------|------|------|----------|
| `0` | 200 | 成功 | — |
| `400` | 400 | 参数或业务校验失败 | 空 factoryName、非 zip、文件过大 |
| `401` | 401 | 未登录或 Token 无效 | 无 Bearer、匿名上传被关闭 |
| `403` | 403 | 无权限 | 角色不足、工站未授权、跨工厂 |
| `404` | 404 | 资源不存在 | 日志/记录/安装包不存在 |
| `500` | 500 | 服务端错误 | 运行环境打包失败等 |

上位机以 **`code == 0` 且 HTTP 2xx** 判定成功（见 `parseEnvelope`）。

---

## 2. 接口列表

以下路径均相对于 **`/api/factory-tool`**。

| 路径 | 方法 | 说明 | 主要调用方 | 鉴权 |
|------|------|------|------------|------|
| `/auth/login` | POST | 登录获取 Token | 上位机、网页 | 无 |
| `/auth/me` | GET | 当前用户信息 | 网页 | Bearer |
| `/auth/logout` | POST | 退出（客户端清 Token） | 上位机、网页 | Bearer |
| `/auth/change-password` | POST | 修改密码 | 网页 | Bearer |
| `/logs/upload` | POST | 上传日志 zip | 上位机 | 可匿名 |
| `/logs` | GET | 日志列表 | 网页 | Bearer |
| `/logs/{id}` | GET | 日志详情 | 网页 | Bearer |
| `/logs/{id}/files/{path}` | GET | 日志文件预览 | 网页 | Bearer |
| `/logs/{id}/download` | GET | 下载日志 zip | 网页 | 二进制 |
| `/test-records` | POST | 上报测试数据 | 上位机 | 可选 |
| `/test-records` | GET | 测试记录列表 | 网页 | Bearer |
| `/test-records/{id}` | GET | 测试记录详情 | 网页 | Bearer |
| `/test-cases/manifest` | GET | 用例包清单 | 上位机 | Bearer |
| `/test-cases/bundle` | GET | 下载用例 zip | 上位机 | Bearer / 二进制 |
| `/test-cases/upload` | POST | 上传用例 zip 并发布 | 上位机（工程师） | Bearer |
| `/admin/test-cases/*` | 多种 | 用例编辑、版本、diff | 网页 | engineer/admin |
| `/host-app/check` | GET | 检查上位机更新 | 上位机 | 无 |
| `/host-app/versions` | GET | 上位机版本列表 | 上位机 | 无 |
| `/host-app/download/{buildId}` | GET | 下载 exe | 上位机 | 二进制 |
| `/host-app/upload` | POST | 上位机上报 exe | 上位机（工程师） | Bearer |
| `/admin/host-app/*` | 多种 | 版本与运行环境管理 | 网页 | admin/engineer |
| `/downloads/summary` | GET | 下载中心摘要 | 网页 | Bearer |
| `/downloads/host-exe` | GET | 下载最新 exe | 网页 | Bearer / 二进制 |
| `/downloads/test-cases` | GET | 下载用例包 | 网页 | Bearer / 二进制 |
| `/downloads/runtime-env` | GET | 下载运行环境 zip | 网页 | Bearer / 二进制 |
| `/analytics/*` | GET | 看板、曲线、良率 | 网页 | Bearer |
| `/admin/meta/*` | 多种 | 工厂、工站字典 | 网页 | Bearer / admin |
| `/admin/users/*` | 多种 | 账号管理 | 网页 | admin |
| `/admin/devices/*` | 多种 | PC 登记 | 网页 | admin |
| `/admin/audit-logins` | GET | 登录审计 | 网页 | admin |
| `/admin/storage/info` | GET | 存储占用 | 网页 | admin |

---

## 3. 接口说明

以下「上位机 → 平台」指客户端发出的 HTTP 请求；「平台 → 上位机」指响应。  
表列为 **字段 | 值 | 说明**；JSON 字段名与代码一致（camelCase）。同一字段多种取值占一行时，**值**与**说明**用顿号对应。

---

### 3.1 `/auth/login` — 登录

**上位机 → 平台**

| 字段 | 值 | 说明 |
|------|-----|------|
| 方法 | `POST` | — |
| 路径 | `/api/factory-tool/auth/login` | — |
| 内容类型 | `application/json` | — |
| 用户名 | — | 账号 |
| 密码 | — | 明文密码（HTTPS 传输） |
| 主机名 | — | 上位机填电脑名；网页默认 `web-console` |
| 设备标识 | 可选 | 与请求头「设备标识」一致 |
| 工站键 | 可选 | 登录时校验工站授权 |

请求体示例：

```json
{
  "username": "operator1",
  "password": "******",
  "hostName": "DESKTOP-PC01",
  "deviceId": "DESKTOP-PC01",
  "stationKey": "m8板厂测试工站"
}
```

**平台 → 上位机**（成功）

| 字段 | 值 | 说明 |
|------|-----|------|
| 业务码 | `0` | 成功 |
| 访问令牌 | — | 写入 `FactoryCloud/Token` |
| 过期时间 | — | ISO8601 UTC，带 `Z` |
| 角色列表 | — | 如 `operator` |
| 工站键列表 | — | 账号已授权工站 |
| 工厂代码 | 可选 | 数据隔离用 |

**平台 → 上位机**（失败）

| 字段 | 值 | 说明 |
|------|-----|------|
| 业务码 | `401` | 账号或密码错误 |
| 业务码 | `403` | 账号锁定、工站未授权 |

---

### 3.2 `/logs/upload` — 日志上传

**上位机 → 平台**

| 字段 | 值 | 说明 |
|------|-----|------|
| 方法 | `POST` | — |
| 路径 | `/api/factory-tool/logs/upload` | — |
| 内容类型 | `multipart/form-data` | — |
| 工厂名 | — | 工厂代码，如 `hz`、`byd` |
| 设备标识 | — | 必填 |
| 工站 | — | 工站显示名 |
| 文件 | — | **zip**（魔数 `PK`），默认最大 **500MB** |
| 主机名 | 可选 | 默认同设备标识 |
| 序列号 | 可选 | SN |
| MAC | 可选 | — |
| 测试结果 | 可选 | 如 `PASS` / `FAIL` |
| 客户端版本 | 可选 | 上位机版本号 |
| 测试记录 ID | 可选 | 关联 `/test-records` 返回的 id |

**平台 → 上位机**（成功）

| 字段 | 值 | 说明 |
|------|-----|------|
| 业务码 | `0` | — |
| 消息 | `上传成功` | — |
| 日志 ID | — | 整数，平台侧归档主键 |

---

### 3.3 `/test-records` — 测试数据上报

**上位机 → 平台**

| 字段 | 值 | 说明 |
|------|-----|------|
| 方法 | `POST` | — |
| 路径 | `/api/factory-tool/test-records` | — |
| 内容类型 | `application/json` | — |
| 工厂名 | — | 必填 |
| 设备标识 | — | 必填 |
| 工站 | — | 必填，工站显示名 |
| 工站键 | 可选 | 工站键 |
| 主机名 | 可选 | — |
| 序列号 | 可选 | — |
| MAC | 可选 | — |
| 测试结果 | 可选 | `PASS`、`FAIL` 等 |
| 机台号 | 可选 | — |
| 产品 | 可选 | 如 `M8` |
| 批次 | 可选 | — |
| 工号 | 可选 | — |
| 客户端版本 | 可选 | — |
| 测试时间 | 可选 | ISO8601，如 `2026-07-14T09:00:00.000Z` |
| 测试项列表 | 可选 | 见下表 |

**测试项（items[]）单条**

| 字段 | 值 | 说明 |
|------|-----|------|
| 名称 | — | 必填，如 `RSSI` |
| 实测值 | 可选 | 字符串 |
| 上限 | 可选 | 卡控上限 |
| 下限 | 可选 | 卡控下限 |
| 标准值 | 可选 | — |
| 单位 | 可选 | 如 `dBm` |
| 结果 | 可选 | `PASS`、`FAIL` |

请求体示例：

```json
{
  "factoryName": "hz",
  "deviceId": "DESKTOP-PC01",
  "station": "自由工站",
  "stationKey": "FREE_WORK",
  "sn": "SN123456",
  "testResult": "PASS",
  "clientVersion": "1.6.2",
  "testedAt": "2026-07-14T09:00:00.000Z",
  "items": [
    {
      "name": "RSSI",
      "value": "-65",
      "minValue": "-90",
      "maxValue": "-50",
      "unit": "dBm",
      "result": "PASS"
    }
  ]
}
```

**平台 → 上位机**（成功）

| 字段 | 值 | 说明 |
|------|-----|------|
| 业务码 | `0` | — |
| 记录 ID | — | 整数 `recordId` |

---

### 3.4 `/test-cases/manifest` 与 `/test-cases/bundle` — 用例同步

#### 3.4.1 查询清单

**上位机 → 平台**

| 字段 | 值 | 说明 |
|------|-----|------|
| 方法 | `GET` | — |
| 路径 | `/api/factory-tool/test-cases/manifest` | — |
| 授权 | Bearer | 须登录 |

**平台 → 上位机**（`data` 主要字段）

| 字段 | 值 | 说明 |
|------|-----|------|
| 包版本 | — | 如 `v3` |
| 工站流程 | 对象 | 键为工站名，值为 `{ "items": ["步骤1.ini", ...] }` |
| 文件列表 | 数组 | 每项含 `path`、`version`、`sha256` |

#### 3.4.2 下载用例包

**上位机 → 平台**

| 字段 | 值 | 说明 |
|------|-----|------|
| 方法 | `GET` | — |
| 路径 | `/api/factory-tool/test-cases/bundle` | — |
| 授权 | Bearer | — |

**平台 → 上位机**

| 字段 | 值 | 说明 |
|------|-----|------|
| 内容类型 | `application/zip` | 无 JSON 包络 |
| 文件名 | `test_case_{版本}.zip` | Content-Disposition |

上位机解压至 exe 同级 `test_case/`，并按 manifest 校验 sha256（见 `test_case_sync_service.cpp`）。

---

### 3.5 `/host-app/check` — 上位机 OTA 检查

**上位机 → 平台**

| 字段 | 值 | 说明 |
|------|-----|------|
| 方法 | `GET` | — |
| 路径 | `/api/factory-tool/host-app/check` | 查询参数 |
| 包名 | — | 默认 `new_production` |
| 构建号 | 可选 | 当前 exe 构建号 |
| 应用版本 | 可选 | 当前 semver |
| 工站键 | 可选 | 预留 |
| 设备标识 | 可选 | 预留 |

示例：`/host-app/check?packageName=new_production&appVersion=1.6.2&buildId=20260616`

**平台 → 上位机**（`data`）

| 字段 | 值 | 说明 |
|------|-----|------|
| 有更新 | `true` / `false` | 服务端版本更新于本机 |
| 本机更新 | `true` / `false` | 本机版本新于服务端（可触发上报） |
| 最新版本信息 | 对象 / 空 | 见下表 |

**最新版本信息（latest）**

| 字段 | 值 | 说明 |
|------|-----|------|
| 应用版本 | — | 如 `1.6.2` |
| 构建号 | — | 如 `20260616` |
| 下载地址 | — | 可为空，上位机拼 `/host-app/download/{buildId}` |
| 文件摘要 | — | SHA256 |
| 强制升级 | `true` / `false` | — |
| 发布说明 | — | 文本 |
| 包名 | — | — |
| 上传时间 | — | ISO8601 |

**下载安装包**：`GET /api/factory-tool/host-app/download/{buildId}?packageName=...&uploadedAt=...` → 二进制 exe。

---

### 3.6 `/host-app/upload` — 上位机版本上报

**上位机 → 平台**（工程师账号）

| 字段 | 值 | 说明 |
|------|-----|------|
| 方法 | `POST` | — |
| 路径 | `/api/factory-tool/host-app/upload` | — |
| 内容类型 | `multipart/form-data` | — |
| 文件 | — | exe |
| 包名 | — | 表单字段 |
| 应用版本 | 可选 | — |
| 构建号 | 可选 | — |
| 强制升级 | 可选 | `0` / `1` |
| 发布说明 | 可选 | — |

---

### 3.7 `/downloads/summary` — 下载中心摘要

**网页 → 平台**

| 字段 | 值 | 说明 |
|------|-----|------|
| 方法 | `GET` | — |
| 路径 | `/api/factory-tool/downloads/summary` | — |
| 授权 | Bearer | 任意登录用户 |

**平台 → 网页**（`data` 概要）

| 字段 | 值 | 说明 |
|------|-----|------|
| 上位机 | 对象 / 空 | 最新 exe 元信息 |
| 测试用例 | 对象 | `bundleVersion`、`fileCount` 等 |
| 运行环境 | 对象 | 运行环境 zip 是否可用及大小 |

配套下载路径：`/downloads/host-exe`、`/downloads/test-cases`、`/downloads/runtime-env`（均为二进制响应）。

---

### 3.8 管理端常用接口（网页）

以下仅列要点；字段详见 OpenAPI `/docs`。

#### 3.8.1 日志查询 `GET /logs`

| 查询参数 | 说明 |
|----------|------|
| 工厂名 | 过滤 |
| 工站 | 模糊 |
| 设备标识 | 模糊 |
| 页码 / 每页条数 | 分页，默认每页 20 |

#### 3.8.2 测试记录 `GET /test-records`

| 查询参数 | 说明 |
|----------|------|
| 工厂名、工站、主机名、SN、MAC、测试结果 | 过滤 |
| 开始时间 / 结束时间 | ISO8601 |
| 页码 / 每页条数 | 分页 |

`GET /test-records/{id}` 返回分项列表及关联日志归档（按时间窗自动匹配）。

#### 3.8.3 元数据

| 路径 | 说明 |
|------|------|
| `GET /admin/meta/factories` | 当前用户可见工厂 |
| `GET /admin/meta/stations` | 工站字典（内置 13 项） |

---

## 4. 上位机配置项

| 配置键 | 说明 |
|--------|------|
| `FactoryCloud/BaseUrl` | 平台根 URL，如 `https://fctp.luteos.com` |
| `FactoryCloud/Token` | 登录后 accessToken |
| `FactoryCloud/DeviceId` | 设备标识，空则取电脑名 |
| `FactoryCloud/HostPackageName` | OTA 包名，默认 `new_production` |
| `Mes/FACTORY` | 上报 `factoryName` |
| `TestOrderMeta/SelectedStationName` | 请求头工站键来源 |

---

## 5. 与设备协议的关系

| 协议 | 用途 | 文档 |
|------|------|------|
| **fwq REST**（本文） | 上位机 ↔ 工厂云：登录、日志、测试数据、用例、OTA | 本文 |
| **dongle 协议**（本文 PHY + AT） | 上位机 ↔ Dongle 模块 | [dongle协议.md](./dongle协议.md) |
| **qroot 等内层串口协议** | Dongle 透传 ↔ 被测设备 | [qroot协议.md](./qroot协议.md) |
| **MES / 三元组** | 上位机 ↔ 客户 MES / 路特云 | 见 `agreement/qtuple` 与 MES 文档 |

fwq 通信**不参与**产测指令下发；产测步骤仍由 `DeviceCmd` + 设备协议完成，完成后可选上报 fwq。
