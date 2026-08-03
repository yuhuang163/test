# mac-address-controller 接口协议 — 三元组 / MAC 服务

> **一句话**：从内网 OpenAPI（Swagger）导出的 `/api/mac-addresses` 全量接口说明（含你打开的 `create_1`）。  
> **读者**：上位机 / 烧录工具对接开发。**前提**：可达 `192.168.200.140:8080`；多数接口需 Basic 鉴权。  
> **来源**：`GET /v3/api-docs`；Swagger UI：`/swagger-ui/index.html#/mac-address-controller/create_1`。

## 快速参考

| 项 | 值 |
|----|-----|
| BaseUrl | `http://192.168.200.140:8080` |
| OpenAPI | `GET /v3/api-docs` |
| Swagger UI | `http://192.168.200.140:8080/swagger-ui/index.html` |
| 锚点接口 create_1 | `POST /api/mac-addresses` |
| 贴片 PCBA SN | `POST /api/mac-addresses/submit-smt-pcba-sn` |
| 贴片 MAC | `POST /api/mac-addresses/submit-smt-mac` |
| 领三元组 | `GET /api/mac-addresses/applyTupleByMac` |
| 原始切片 JSON | `docs/开发参考资料/_swagger_v3_mac.json` |

## 1. 范围

### 包含
- 路径前缀 `/api/mac-addresses` 下全部方法（Swagger `mac-address-controller`）
- 参数、Body Schema、响应码与字段表

### 不包含（边界）
- product-config / batch / 用户管理等其它 controller
- 服务端贴片卡控实现（仅契约；业务 `message` 以接口返回为准）

## 2. 接口一览

| Method | Path | operationId | summary |
|--------|------|-------------|---------|
| `POST` | `/api/mac-addresses` | `create_1` ← **create_1** | 产线流程 |
| `POST` | `/api/mac-addresses/apply-oem-triplet` | `applyByMacProductAndBatch` | 通过申请生成记录 |
| `GET` | `/api/mac-addresses/apply-single` | `applySingleMac` | 申请单个MAC地址 |
| `POST` | `/api/mac-addresses/apply-sn-by-mac-batch` | `applySnByMacAndBatch` | 根据MAC和批次号申请SN |
| `POST` | `/api/mac-addresses/apply-wifi-triplet` | `applyWifiByMacProductAndBatch` | 通过申请生成记录 |
| `GET` | `/api/mac-addresses/applyTupleByMac` | `applyTupleByMac` | 根据MAC地址获取三元组 |
| `GET` | `/api/mac-addresses/auth` | `auth` | 认证接口 |
| `POST` | `/api/mac-addresses/batch` | `createBatch` | 批量创建MAC地址 |
| `POST` | `/api/mac-addresses/check-sign` | `checkSign` | 签名校验接口 |
| `GET` | `/api/mac-addresses/get-pcba-sn-by-mac` | `getPcbaSnByMac` | 根据MAC地址查询pcbaSn |
| `GET` | `/api/mac-addresses/get-sn-by-mac` | `getSnByMac` | 根据MAC地址获取SN |
| `GET` | `/api/mac-addresses/getHex` | `getHex` | 根据MAC地址获设置Mac的Hex命令 |
| `POST` | `/api/mac-addresses/import-it` | `importToItTable` | 导入MAC地址到IT表 |
| `GET` | `/api/mac-addresses/reset-group-to-record` | `resetGroupToQc` | 重置QC状态为已记录 |
| `POST` | `/api/mac-addresses/reset-sn` | `resetSn` | 重置SN |
| `POST` | `/api/mac-addresses/reset-status` | `resetStatus` | 重置状态 |
| `POST` | `/api/mac-addresses/submit-m9-qc-mac` | `submitM9AssemblyMac` | 组装厂M9吸奶器SN提交 |
| `POST` | `/api/mac-addresses/submit-smt-mac` | `submitSmtMac` | 贴片厂MAC地址提交 |
| `POST` | `/api/mac-addresses/submit-smt-pcba-sn` | `submitSmtPcbaSn` | 贴片厂PCBA SN提交 |
| `GET` | `/api/mac-addresses/sync-device-info` | `syncDeviceInfo` | 同步设备数据 |
| `POST` | `/api/mac-addresses/update-oem-record-status` | `updateOemRecordStatus` | 更新三元组烧录状态 |
| `POST` | `/api/mac-addresses/update-wifi-record-status` | `updateWifiRecordStatus` | 更新三元组烧录状态 |
| `POST` | `/api/mac-addresses/upload` | `uploadFile` | 通过文件上传创建MAC地址 |
| `POST` | `/api/mac-addresses/upload-pcba-sn` | `uploadPcbaSn` | 上传PCBA SN |
| `GET` | `/api/mac-addresses/uploadTripletToS3` | `uploadTripletToS3` | 上传三元组到S3 |

## 3. 接口详情

### `POST /api/mac-addresses`

- **operationId**：`create_1`
- **summary**：产线流程

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`IotDeviceApplyRecordEntity`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | integer(int64) | 否 |  |
| `mac` | string | 否 |  |
| `sn` | string | 否 |  |
| `pcbaSn` | string | 否 |  |
| `productKey` | string | 否 |  |
| `deviceName` | string | 否 |  |
| `status` | string enum['UNRECORDED', 'RECORDED', 'QC_PASSED'] | 否 |  |
| `createdTime` | string(date-time) | 否 |  |
| `updatedTime` | string(date-time) | 否 |  |
| `updatedBy` | string | 否 |  |
| `batchNumber` | string | 否 |  |
| `factoryCode` | string | 否 |  |
| `deleted` | boolean | 否 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 成功 | MacAddressResponse |
| `201` | Created | MacAddressResponse |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `success` | integer(int32) | 否 |  |
| `mac` | string | 否 |  |
| `productKey` | string | 否 |  |
| `deviceName` | string | 否 |  |
| `deviceSecret` | string | 否 |  |
| `sn` | string | 否 |  |
| `pcbaSn` | string | 否 |  |
| `status` | integer(int32) | 否 |  |
| `availableCount` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |

**201 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `success` | integer(int32) | 否 |  |
| `mac` | string | 否 |  |
| `productKey` | string | 否 |  |
| `deviceName` | string | 否 |  |
| `deviceSecret` | string | 否 |  |
| `sn` | string | 否 |  |
| `pcbaSn` | string | 否 |  |
| `status` | integer(int32) | 否 |  |
| `availableCount` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |

### `POST /api/mac-addresses/apply-oem-triplet`

- **operationId**：`applyByMacProductAndBatch`
- **summary**：通过申请生成记录

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`ApplyOemTripletRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `mac` | string | 是 | MAC地址 示例=`00:11:22:33:44:55` |
| `batchNumber` | string | 是 | 批次号 示例=`BN20230501` |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 申请成功 | RMacAddressResponse |
| `400` | 参数错误 | RMacAddressResponse |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | object | 否 |  |
| `data.success` | integer(int32) | 否 |  |
| `data.mac` | string | 否 |  |
| `data.productKey` | string | 否 |  |
| `data.deviceName` | string | 否 |  |
| `data.deviceSecret` | string | 否 |  |
| `data.sn` | string | 否 |  |
| `data.pcbaSn` | string | 否 |  |
| `data.status` | integer(int32) | 否 |  |
| `data.availableCount` | integer(int32) | 否 |  |
| `data.msg` | string | 否 |  |

### `GET /api/mac-addresses/apply-single`

- **operationId**：`applySingleMac`
- **summary**：申请单个MAC地址

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `batchNumber` | string | 是 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 申请成功 | MacAddressResponse |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `success` | integer(int32) | 否 |  |
| `mac` | string | 否 |  |
| `productKey` | string | 否 |  |
| `deviceName` | string | 否 |  |
| `deviceSecret` | string | 否 |  |
| `sn` | string | 否 |  |
| `pcbaSn` | string | 否 |  |
| `status` | integer(int32) | 否 |  |
| `availableCount` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |

### `POST /api/mac-addresses/apply-sn-by-mac-batch`

- **operationId**：`applySnByMacAndBatch`
- **summary**：根据MAC和批次号申请SN

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `mac` | string | 是 |  |
| query | `batchNumber` | string | 是 |  |
| query | `position` | string | 否 |  |
| query | `sku` | string | 否 |  |
| query | `pVersion` | string | 否 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `500` | 申请失败 | RMacAddressResponse |
| `200` | 申请成功 | RMacAddressResponse |
| `400` | 参数错误 | RMacAddressResponse |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | object | 否 |  |
| `data.success` | integer(int32) | 否 |  |
| `data.mac` | string | 否 |  |
| `data.productKey` | string | 否 |  |
| `data.deviceName` | string | 否 |  |
| `data.deviceSecret` | string | 否 |  |
| `data.sn` | string | 否 |  |
| `data.pcbaSn` | string | 否 |  |
| `data.status` | integer(int32) | 否 |  |
| `data.availableCount` | integer(int32) | 否 |  |
| `data.msg` | string | 否 |  |

### `POST /api/mac-addresses/apply-wifi-triplet`

- **operationId**：`applyWifiByMacProductAndBatch`
- **summary**：通过申请生成记录

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`ApplyWifiTripletRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `mac` | string | 是 | MAC地址 示例=`00:11:22:33:44:55` |
| `batchNumber` | string | 是 | 批次号 示例=`BN20230501` |
| `sn` | string | 是 | SN码 示例=`SN20230501` |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 申请成功 | RMacAddressResponse |
| `400` | 参数错误 | RMacAddressResponse |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | object | 否 |  |
| `data.success` | integer(int32) | 否 |  |
| `data.mac` | string | 否 |  |
| `data.productKey` | string | 否 |  |
| `data.deviceName` | string | 否 |  |
| `data.deviceSecret` | string | 否 |  |
| `data.sn` | string | 否 |  |
| `data.pcbaSn` | string | 否 |  |
| `data.status` | integer(int32) | 否 |  |
| `data.availableCount` | integer(int32) | 否 |  |
| `data.msg` | string | 否 |  |

### `GET /api/mac-addresses/applyTupleByMac`

- **operationId**：`applyTupleByMac`
- **summary**：根据MAC地址获取三元组

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `mac` | string | 是 |  |
| query | `position` | string | 是 |  |
| query | `sku` | string | 否 |  |
| query | `pVersion` | string | 否 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 获取成功 | MacAddressResponse |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `success` | integer(int32) | 否 |  |
| `mac` | string | 否 |  |
| `productKey` | string | 否 |  |
| `deviceName` | string | 否 |  |
| `deviceSecret` | string | 否 |  |
| `sn` | string | 否 |  |
| `pcbaSn` | string | 否 |  |
| `status` | integer(int32) | 否 |  |
| `availableCount` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |

### `GET /api/mac-addresses/auth`

- **operationId**：`auth`
- **summary**：认证接口

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 认证成功 | R |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | object | 否 |  |

### `POST /api/mac-addresses/batch`

- **operationId**：`createBatch`
- **summary**：批量创建MAC地址

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `status` | integer(int32) | 是 |  |

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `201` | 批量创建成功 | BatchResult |

**201 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `successCount` | integer(int32) | 否 |  |
| `failedCount` | integer(int32) | 否 |  |

### `POST /api/mac-addresses/check-sign`

- **operationId**：`checkSign`
- **summary**：签名校验接口

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`CheckSignRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `productKey` | string | 是 | 产品密钥 示例=`product123` |
| `deviceName` | string | 是 | 设备名称 示例=`device001` |
| `sign` | string | 是 | 签名 示例=`signature123` |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 校验成功 | RBoolean |
| `400` | 参数错误 | RBoolean |
| `500` | 校验失败 | RBoolean |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | boolean | 否 |  |

### `GET /api/mac-addresses/get-pcba-sn-by-mac`

- **operationId**：`getPcbaSnByMac`
- **summary**：根据MAC地址查询pcbaSn

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `mac` | string | 是 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `400` | 参数错误 | RString |
| `404` | MAC地址不存在 | RString |
| `200` | 查询成功 | RString |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | string | 否 |  |

### `GET /api/mac-addresses/get-sn-by-mac`

- **operationId**：`getSnByMac`
- **summary**：根据MAC地址获取SN

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `mac` | string | 是 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 获取成功 | RString |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | string | 否 |  |

### `GET /api/mac-addresses/getHex`

- **operationId**：`getHex`
- **summary**：根据MAC地址获设置Mac的Hex命令

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `mac` | string | 是 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 获取成功 | string |

### `POST /api/mac-addresses/import-it`

- **operationId**：`importToItTable`
- **summary**：导入MAC地址到IT表

#### 请求体

- **必填**：否
- **Content-Type**：`multipart/form-data`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `file` | string(binary) | 是 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `201` | 文件导入处理成功 | BatchResult |

**201 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `successCount` | integer(int32) | 否 |  |
| `failedCount` | integer(int32) | 否 |  |

### `GET /api/mac-addresses/reset-group-to-record`

- **operationId**：`resetGroupToQc`
- **summary**：重置QC状态为已记录

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `batchNumber` | string | 是 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 重置成功 | RInteger |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | integer(int32) | 否 |  |

### `POST /api/mac-addresses/reset-sn`

- **operationId**：`resetSn`
- **summary**：重置SN

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `sn` | string | 是 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `500` | 重置失败 | RString |
| `200` | 重置成功 | RString |
| `400` | 参数错误 | RString |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | string | 否 |  |

### `POST /api/mac-addresses/reset-status`

- **operationId**：`resetStatus`
- **summary**：重置状态

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`IotDeviceApplyRecordEntity`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | integer(int64) | 否 |  |
| `mac` | string | 否 |  |
| `sn` | string | 否 |  |
| `pcbaSn` | string | 否 |  |
| `productKey` | string | 否 |  |
| `deviceName` | string | 否 |  |
| `status` | string enum['UNRECORDED', 'RECORDED', 'QC_PASSED'] | 否 |  |
| `createdTime` | string(date-time) | 否 |  |
| `updatedTime` | string(date-time) | 否 |  |
| `updatedBy` | string | 否 |  |
| `batchNumber` | string | 否 |  |
| `factoryCode` | string | 否 |  |
| `deleted` | boolean | 否 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 状态重置成功 | MacAddressResponse |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `success` | integer(int32) | 否 |  |
| `mac` | string | 否 |  |
| `productKey` | string | 否 |  |
| `deviceName` | string | 否 |  |
| `deviceSecret` | string | 否 |  |
| `sn` | string | 否 |  |
| `pcbaSn` | string | 否 |  |
| `status` | integer(int32) | 否 |  |
| `availableCount` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |

### `POST /api/mac-addresses/submit-m9-qc-mac`

- **operationId**：`submitM9AssemblyMac`
- **summary**：组装厂M9吸奶器SN提交

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`SubmitM9AssemblyMacRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `mac` | string | 否 |  |
| `sn` | string | 否 |  |
| `factoryCode` | string | 否 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `500` | 提交失败 | RString |
| `200` | 提交成功 | RString |
| `400` | 参数错误 | RString |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | string | 否 |  |

### `POST /api/mac-addresses/submit-smt-mac`

- **operationId**：`submitSmtMac`
- **summary**：贴片厂MAC地址提交

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`SubmitSmtMacRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `mac` | string | 否 |  |
| `factoryCode` | string | 否 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `500` | 提交失败 | RString |
| `200` | 提交成功 | RString |
| `400` | 参数错误 | RString |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | string | 否 |  |

### `POST /api/mac-addresses/submit-smt-pcba-sn`

- **operationId**：`submitSmtPcbaSn`
- **summary**：贴片厂PCBA SN提交

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`SubmitSmtPcbaSnRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `pcbaSn` | string | 否 |  |
| `factoryCode` | string | 否 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `500` | 提交失败 | RString |
| `200` | 提交成功 | RString |
| `400` | 参数错误 | RString |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | string | 否 |  |

### `GET /api/mac-addresses/sync-device-info`

- **operationId**：`syncDeviceInfo`
- **summary**：同步设备数据

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `batchNumber` | string | 否 |  |
| query | `startTime` | string(date-time) | 否 |  |
| query | `endTime` | string(date-time) | 否 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 同步成功 | R |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | object | 否 |  |

### `POST /api/mac-addresses/update-oem-record-status`

- **operationId**：`updateOemRecordStatus`
- **summary**：更新三元组烧录状态

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`UpdateOemRecordStatusRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `mac` | string | 是 | MAC地址 示例=`00:11:22:33:44:55` |
| `batchNumber` | string | 是 | 批次号 示例=`BN20230501` |
| `sn` | string | 是 | SN 示例=`SN20230501` |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `500` | 更新失败 | RMacAddressResponse |
| `400` | 参数错误 | RMacAddressResponse |
| `200` | 更新成功 | RMacAddressResponse |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | object | 否 |  |
| `data.success` | integer(int32) | 否 |  |
| `data.mac` | string | 否 |  |
| `data.productKey` | string | 否 |  |
| `data.deviceName` | string | 否 |  |
| `data.deviceSecret` | string | 否 |  |
| `data.sn` | string | 否 |  |
| `data.pcbaSn` | string | 否 |  |
| `data.status` | integer(int32) | 否 |  |
| `data.availableCount` | integer(int32) | 否 |  |
| `data.msg` | string | 否 |  |

### `POST /api/mac-addresses/update-wifi-record-status`

- **operationId**：`updateWifiRecordStatus`
- **summary**：更新三元组烧录状态

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`UpdateWifiRecordStatusRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `batchNumber` | string | 是 | 批次号 示例=`BN20230501` |
| `sn` | string | 是 | SN 示例=`SN20230501` |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `500` | 更新失败 | RMacAddressResponse |
| `400` | 参数错误 | RMacAddressResponse |
| `200` | 更新成功 | RMacAddressResponse |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | object | 否 |  |
| `data.success` | integer(int32) | 否 |  |
| `data.mac` | string | 否 |  |
| `data.productKey` | string | 否 |  |
| `data.deviceName` | string | 否 |  |
| `data.deviceSecret` | string | 否 |  |
| `data.sn` | string | 否 |  |
| `data.pcbaSn` | string | 否 |  |
| `data.status` | integer(int32) | 否 |  |
| `data.availableCount` | integer(int32) | 否 |  |
| `data.msg` | string | 否 |  |

### `POST /api/mac-addresses/upload`

- **operationId**：`uploadFile`
- **summary**：通过文件上传创建MAC地址

#### 参数

| 位置 | 名称 | 类型 | 必填 | 说明 |
|------|------|------|------|------|
| query | `status` | integer(int32) | 是 |  |

#### 请求体

- **必填**：否
- **Content-Type**：`multipart/form-data`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `file` | string(binary) | 是 |  |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `201` | 文件上传处理成功 | BatchResult |

**201 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `successCount` | integer(int32) | 否 |  |
| `failedCount` | integer(int32) | 否 |  |

### `POST /api/mac-addresses/upload-pcba-sn`

- **operationId**：`uploadPcbaSn`
- **summary**：上传PCBA SN

#### 请求体

- **必填**：是
- **Content-Type**：`application/json`
- **Schema**：`UploadPcbaSnRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `pcbaSn` | string | 是 | PCBA序列号 示例=`ABCDEF123456789012` |

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 上传成功 | RMacAddressResponse |
| `400` | 参数错误 | RMacAddressResponse |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | object | 否 |  |
| `data.success` | integer(int32) | 否 |  |
| `data.mac` | string | 否 |  |
| `data.productKey` | string | 否 |  |
| `data.deviceName` | string | 否 |  |
| `data.deviceSecret` | string | 否 |  |
| `data.sn` | string | 否 |  |
| `data.pcbaSn` | string | 否 |  |
| `data.status` | integer(int32) | 否 |  |
| `data.availableCount` | integer(int32) | 否 |  |
| `data.msg` | string | 否 |  |

### `GET /api/mac-addresses/uploadTripletToS3`

- **operationId**：`uploadTripletToS3`
- **summary**：上传三元组到S3

#### 响应

| HTTP | 说明 | Schema |
|------|------|--------|
| `200` | 获取成功 | RString |

**200 响应字段**（`*/*`）

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | string | 否 |  |

## 4. Schema 定义（本控制器引用）

### `ApplyOemTripletRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `mac` | string | 是 | MAC地址 示例=`00:11:22:33:44:55` |
| `batchNumber` | string | 是 | 批次号 示例=`BN20230501` |

### `ApplyWifiTripletRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `mac` | string | 是 | MAC地址 示例=`00:11:22:33:44:55` |
| `batchNumber` | string | 是 | 批次号 示例=`BN20230501` |
| `sn` | string | 是 | SN码 示例=`SN20230501` |

### `BatchResult`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `successCount` | integer(int32) | 否 |  |
| `failedCount` | integer(int32) | 否 |  |

### `CheckSignRequest`

签名校验请求参数

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `productKey` | string | 是 | 产品密钥 示例=`product123` |
| `deviceName` | string | 是 | 设备名称 示例=`device001` |
| `sign` | string | 是 | 签名 示例=`signature123` |

### `IotDeviceApplyRecordEntity`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `id` | integer(int64) | 否 |  |
| `mac` | string | 否 |  |
| `sn` | string | 否 |  |
| `pcbaSn` | string | 否 |  |
| `productKey` | string | 否 |  |
| `deviceName` | string | 否 |  |
| `status` | string enum['UNRECORDED', 'RECORDED', 'QC_PASSED'] | 否 |  |
| `createdTime` | string(date-time) | 否 |  |
| `updatedTime` | string(date-time) | 否 |  |
| `updatedBy` | string | 否 |  |
| `batchNumber` | string | 否 |  |
| `factoryCode` | string | 否 |  |
| `deleted` | boolean | 否 |  |

### `MacAddressResponse`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `success` | integer(int32) | 否 |  |
| `mac` | string | 否 |  |
| `productKey` | string | 否 |  |
| `deviceName` | string | 否 |  |
| `deviceSecret` | string | 否 |  |
| `sn` | string | 否 |  |
| `pcbaSn` | string | 否 |  |
| `status` | integer(int32) | 否 |  |
| `availableCount` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |

### `R`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | object | 否 |  |

### `RBoolean`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | boolean | 否 |  |

### `RInteger`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | integer(int32) | 否 |  |

### `RMacAddressResponse`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | object | 否 |  |
| `data.success` | integer(int32) | 否 |  |
| `data.mac` | string | 否 |  |
| `data.productKey` | string | 否 |  |
| `data.deviceName` | string | 否 |  |
| `data.deviceSecret` | string | 否 |  |
| `data.sn` | string | 否 |  |
| `data.pcbaSn` | string | 否 |  |
| `data.status` | integer(int32) | 否 |  |
| `data.availableCount` | integer(int32) | 否 |  |
| `data.msg` | string | 否 |  |

### `RString`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `code` | integer(int32) | 否 |  |
| `msg` | string | 否 |  |
| `data` | string | 否 |  |

### `SubmitM9AssemblyMacRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `mac` | string | 否 |  |
| `sn` | string | 否 |  |
| `factoryCode` | string | 否 |  |

### `SubmitSmtMacRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `mac` | string | 否 |  |
| `factoryCode` | string | 否 |  |

### `SubmitSmtPcbaSnRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `pcbaSn` | string | 否 |  |
| `factoryCode` | string | 否 |  |

### `UpdateOemRecordStatusRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `mac` | string | 是 | MAC地址 示例=`00:11:22:33:44:55` |
| `batchNumber` | string | 是 | 批次号 示例=`BN20230501` |
| `sn` | string | 是 | SN 示例=`SN20230501` |

### `UpdateWifiRecordStatusRequest`

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `batchNumber` | string | 是 | 批次号 示例=`BN20230501` |
| `sn` | string | 是 | SN 示例=`SN20230501` |

### `UploadPcbaSnRequest`

上传PCBA SN请求参数

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `pcbaSn` | string | 是 | PCBA序列号 示例=`ABCDEF123456789012` |

## 5. 验证

1. 浏览器打开 Swagger UI，展开 `mac-address-controller` → `POST /api/mac-addresses`（operationId=`create_1`）。
2. `GET /v3/api-docs` 中检索 `"operationId":"create_1"`。
3. 贴片：检索 `submit-smt-pcba-sn` / `submit-smt-mac`。
4. 领三元组：`GET /api/mac-addresses/applyTupleByMac`（上位机 `QTupleService::applyTupleByMacImpl`）。

## 6. 常见问题

| 现象 | 原因 | 处理 |
|------|------|------|
| 提示与贴片 / PCBA SN 相关 | 业务校验写在服务端返回 `message` | 查 `applyTupleByMac` / `submit-smt-pcba-sn` 响应 |
| 401 | 未带 Basic 鉴权 | 先调 `GET /api/mac-addresses/auth` |
| Swagger 中文乱码 | 页面编码 | 以本 md / UTF-8 JSON 为准 |

---

*由 `scripts/_export_mac_swagger_md.py` 从 `http://192.168.200.140:8080/v3/api-docs` 导出；共 25 个接口。*
