#ifndef PLATFORM_TEST_CASE_SEND_DISPATCH_H
#define PLATFORM_TEST_CASE_SEND_DISPATCH_H
#include "cmd_catalog_base.h"
#include "test_case_types.h"
#include <QSettings>
#include <QStringList>

/** Send 通道 → CmdManifestCatalog 分发；validate / ini 读写共用。 */
namespace TestCaseSendDispatch {
void appendCatalogValidationErrors(const TestCaseSend& send, QStringList& errors);
void appendModbusScpiValidationErrors(const TestCaseSend& send, QStringList& errors);

/** 从 DeviceCmd 名推断 channel / fixtureProtocol（旧 ini 缺 Send/Channel 时用）。 */
bool inferChannelFromDeviceCmd(const QString& deviceCmd, TestCaseSendChannel& channel,
                               TestCaseFixtureProtocol& fixtureProtocol);

/** 旧配置：USB 摄像头指令写在非 Fixture 通道时并入治具协议。 */
void normalizeLegacyUsbCameraSend(TestCaseSend& send);
void loadSendParamFromIni(const QSettings& ini, TestCaseSend& send, bool mergeProductHookParamMap = false);
void writeSendParamToIni(QSettings& ini, const TestCaseSend& send);
} // namespace TestCaseSendDispatch
#endif // PLATFORM_TEST_CASE_SEND_DISPATCH_H
