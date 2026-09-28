#ifndef HOST_OTA_VERSION_H
#define HOST_OTA_VERSION_H

/**
 * 上位机整包 OTA 版本 — 编译默认值（非运行时唯一来源）。
 *
 * 运行时优先读本地 Ini：FactoryCloud/HostAppVersion（上位机设置.local.ini）
 * （首次为空时用本宏落盘；「检查更新」可改；OTA 安装成功也会写入）。
 *
 * 后缀按 Git 分支（写入本地号时同样遵守）：
 * - 主线 develop → -p（如 1.6.37-p）。
 * - 其他分支 → -d（如 1.6.36-d）。
 *
 * buildId 由编译时刻自动生成（yyyyMMdd-HHmmss），见 FactoryCloudClient::buildId()。
 *
 * 勿把本文件加入 AbIni.h / PRECOMPILED_HEADER。
 */
#define HOST_OTA_APP_VERSION "1.6.36"
#define HOST_OTA_BUILD_ID "2026089"

#endif // HOST_OTA_VERSION_H
