#ifndef PLATFORM_TEST_CASE_PATHS_H
#define PLATFORM_TEST_CASE_PATHS_H

#include <QString>

namespace TestCasePaths {
QString rootDir();
QString stepsDir();
QString profilesDir();
/** profiles 子目录名：工站中文显示名（与 FlowStations 一致） */
QString profileFolderName(const QString& stationKey);
QString profileDir(const QString& stationKey);
QString profileMetaPath(const QString& stationKey);
QString profileFlowPath(const QString& stationKey);
QString profileStepOverridePath(const QString& stationKey, const QString& stepId);
QString stepLibraryPath(const QString& stepId);
QString flowIniPath();
QString caseIniPath(const QString& caseName);
/** 步骤是否在 steps 库或工站 profiles/{key}/steps 覆盖中存在 */
bool stepIniExistsForStation(const QString& stationKey, const QString& stepId);
QString flowIniFileName();
bool ensureRootDir();
bool isValidCaseFileName(const QString& name, QString* errorOut = nullptr);
bool isReservedCaseName(const QString& name);
} // namespace TestCasePaths

#endif // PLATFORM_TEST_CASE_PATHS_H
