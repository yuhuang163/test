#ifndef HOST_OTA_VERSION_H
#define HOST_OTA_VERSION_H

/**
 * 上位机整包 OTA 版本（唯一维护处）。
 *
 * 注意：
 * - 勿把本文件加入 AbIni.h / PRECOMPILED_HEADER，否则改号会触发全量编译。
 * - 仅 factory_cloud_client.cpp 等少数源文件 include；改完增量编即可。
 * - buildId 须单调递增，格式与云端比较一致：yyyyMMdd 或 yyyyMMdd-N（同日第 N 次发版）。
 * - appVersion 为展示/比较用语义版本（如 1.6.5）；与工站 DEBUG_VER / FREE_VER 窗口标题无关。
 */
#define HOST_OTA_APP_VERSION "1.6.5"
#define HOST_OTA_BUILD_ID "20260723"

#endif // HOST_OTA_VERSION_H
