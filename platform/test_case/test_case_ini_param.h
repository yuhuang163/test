#ifndef PLATFORM_TEST_CASE_INI_PARAM_H
#define PLATFORM_TEST_CASE_INI_PARAM_H

#include "test_case_types.h"

#include <QSettings>
#include <QString>
#include <QVariant>
#include <QVariantMap>

/** 步骤 ini Send/Param 读写，仅 test_case 编译单元内部使用。 */
QString sendParamIniPrefix();
QString sendParamIniKey(const QString& leafKey);
void removeKeysWithPrefix(QSettings& s, const QString& prefix);
void removeSendParamKeys(QSettings& s);
void writeSendParamMap(QSettings& s, const QVariantMap& map);
void writeSendParamLeaf(QSettings& s, const QString& leafKey, const QVariant& value);
QVariant readJsonMap(const QSettings& s, const QString& prefix);
void writeJsonMap(QSettings& s, const QString& prefix, const QVariant& value);
QVariant normalizeScpiModbusParamFromMap(const QVariantMap& map);
void writeScpiModbusParamToIni(QSettings& ini, const QVariant& param);
QVariant readSendScopedParam(const QSettings& settings, const QString& leafKey, const QVariant& defaultValue);
QVariantMap readSendParamMap(const QSettings& settings);
void mergeSendParamMapInto(QVariant& param, const QVariantMap& extra);
bool hookUsesGenericSendParamMap(const TestCaseDefinition& def);
void writeGenericHookSendParamMap(QSettings& ini, const TestCaseDefinition& def);
int jsonMapIntValue(const QVariantMap& map, int defaultValue = 0);
QVariantMap jsonMapWithLegacyInt(const QSettings& settings);
bool overlayHasSendParamKeys(const QSettings& overlay);

#endif // PLATFORM_TEST_CASE_INI_PARAM_H
