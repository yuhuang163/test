#include "test_case_send_dispatch.h"

#include "test_case.h"
#include "test_case_ini_param.h"

#include "modbus_device_catalog.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace TestCaseSendDispatch {

namespace {

struct CatalogBinding {
    CmdManifestCatalog& (*catalogFn)();
    const char* invalidCmdError;
    const char* actionMismatchError;
    const char* missingSchemaError;
    bool checkParamSchema = true;
    bool forceActionFromCatalog = false;
    bool clearParamOnLoad = false;
};

CmdManifestCatalog& deviceCatalogFn() {
    return DeviceCmdCatalog::catalog();
}

CmdManifestCatalog& dongleCatalogFn() {
    return DongleCmdCatalog::catalog();
}

CmdManifestCatalog& cloudCatalogFn() {
    return TupleCmdCatalog::catalog();
}

CmdManifestCatalog& productSerialCatalogFn() {
    return ProductSerialCmdCatalog::catalog();
}

CmdManifestCatalog& asd9026aCatalogFn() {
    return Asd9026aCmdCatalog::catalog();
}

CmdManifestCatalog& xwdCatalogFn() {
    return XwdRawFixtureCmdCatalog::catalog();
}

CmdManifestCatalog& jieliCatalogFn() {
    return JieliBtBoxCmdCatalog::catalog();
}

CmdManifestCatalog& usbCameraCatalogFn() {
    return UsbCameraCmdCatalog::catalog();
}

CmdManifestCatalog& vesLightCatalogFn() {
    return VesLightCmdCatalog::catalog();
}

CmdManifestCatalog& fixturePcbaCatalogFn() {
    return FixturePcbaCmdCatalog::catalog();
}

const CatalogBinding* catalogBinding(TestCaseSendChannel channel, TestCaseFixtureProtocol fixtureProtocol) {
    static const CatalogBinding kDevice = {deviceCatalogFn,
                                           u8"产品测试指令无效",
                                           u8"产品指令与操作方式不匹配",
                                           u8"该指令尚未配置参数模板，请联系工程师"};
    static const CatalogBinding kDongle = {dongleCatalogFn,
                                         u8"Dongle 测试指令无效",
                                         u8"Dongle 指令与操作方式不匹配",
                                         u8"该 Dongle 指令尚未配置参数模板，请联系工程师"};
    static const CatalogBinding kCloud = {cloudCatalogFn,
                                          u8"云端测试指令无效",
                                          u8"云端指令与操作方式不匹配",
                                          u8"该云端指令尚未配置参数模板，请联系工程师"};
    static const CatalogBinding kProductSerial = {productSerialCatalogFn,
                                                u8"产品串口测试指令无效",
                                                u8"产品串口指令仅支持「设置」",
                                                nullptr,
                                                false,
                                                true,
                                                true};
    static const CatalogBinding kAsd9026a = {asd9026aCatalogFn,
                                           u8"ASD9026A 治具指令无效",
                                           u8"ASD9026A 指令与操作方式不匹配",
                                           u8"该 ASD9026A 指令尚未配置参数模板，请联系工程师"};
    static const CatalogBinding kXwd = {xwdCatalogFn,
                                        u8"XWD治具指令无效",
                                        u8"XWD治具指令与操作方式不匹配",
                                        u8"该 XWD治具指令尚未配置参数模板，请联系工程师"};
    static const CatalogBinding kJieli = {jieliCatalogFn,
                                        u8"杰理蓝牙盒子指令无效",
                                        u8"杰理蓝牙盒子指令与操作方式不匹配",
                                        u8"该杰理蓝牙盒子指令尚未配置参数模板，请联系工程师"};
    static const CatalogBinding kUsbCamera = {usbCameraCatalogFn,
                                            u8"USB 摄像头测试指令无效",
                                            u8"USB 摄像头指令与操作方式不匹配（请选「读取」）",
                                            u8"该 USB 摄像头指令尚未配置参数模板，请联系工程师"};
    static const CatalogBinding kVesLight = {vesLightCatalogFn,
                                           u8"VES 光源测试指令无效",
                                           u8"VES 光源指令与操作方式不匹配（请选「设置」）",
                                           u8"该 VES 光源指令尚未配置参数模板，请联系工程师"};
    static const CatalogBinding kFixturePcba = {fixturePcbaCatalogFn,
                                              u8"治具 PCBA 测试指令无效",
                                              u8"治具指令与操作方式不匹配",
                                              u8"该治具指令尚未配置参数模板，请联系工程师"};

    switch (channel) {
    case TestCaseSendChannel::Product:
        return &kDevice;
    case TestCaseSendChannel::Dongle:
        return &kDongle;
    case TestCaseSendChannel::Cloud:
        return &kCloud;
    case TestCaseSendChannel::ProductSerial:
        return &kProductSerial;
    case TestCaseSendChannel::Fixture:
        switch (fixtureProtocol) {
        case TestCaseFixtureProtocol::Asd9026a:
            return &kAsd9026a;
        case TestCaseFixtureProtocol::Xwd:
            return &kXwd;
        case TestCaseFixtureProtocol::JieliBtBox:
            return &kJieli;
        case TestCaseFixtureProtocol::UsbCamera:
            return &kUsbCamera;
        case TestCaseFixtureProtocol::VesLight:
            return &kVesLight;
        case TestCaseFixtureProtocol::Pcba:
            return &kFixturePcba;
        default:
            return nullptr;
        }
    default:
        return nullptr;
    }
}

bool tryInferFromCatalog(CmdManifestCatalog& catalog, const QString& deviceCmd, TestCaseSendChannel channel,
                         TestCaseFixtureProtocol fixtureProtocol, TestCaseSendChannel& outChannel,
                         TestCaseFixtureProtocol& outFixtureProtocol) {
    int cmd = 0;
    if (!catalog.cmdFromName(deviceCmd, cmd))
        return false;
    outChannel = channel;
    outFixtureProtocol = fixtureProtocol;
    return true;
}

void loadModbusScpiParamFromIni(const QSettings& ini, TestCaseSend& send) {
    const QVariantMap paramMap = readSendParamMap(ini);
    if (!paramMap.isEmpty()) {
        send.param = normalizeScpiModbusParamFromMap(paramMap);
        return;
    }
    QVariant val = ini.value(QStringLiteral("Send/Param"));
    if (!val.isValid())
        val = readSendScopedParam(ini, QStringLiteral("value"), QVariant());
    if (!val.isValid())
        val = readSendScopedParam(ini, QStringLiteral("int"), QVariant());
    if (!val.isValid())
        val = readSendScopedParam(ini, QStringLiteral("string"), QVariant());
    send.param = val;
}

} // namespace

void appendCatalogValidationErrors(const TestCaseSend& send, QStringList& errors) {
    const CatalogBinding* binding = catalogBinding(send.channel, send.fixtureProtocol);
    if (!binding) {
        if (send.channel == TestCaseSendChannel::Fixture)
            errors.append(QStringLiteral("治具协议类型无效"));
        return;
    }
    int cmd = 0;
    CmdManifestCatalog& catalog = binding->catalogFn();
    if (!catalog.cmdFromName(send.deviceCmd, cmd)) {
        errors.append(QString::fromUtf8(binding->invalidCmdError));
        return;
    }
    if (!catalog.isCmdForAction(cmd, send.action)) {
        errors.append(QString::fromUtf8(binding->actionMismatchError));
        return;
    }
    if (!binding->checkParamSchema)
        return;
    DeviceCmdParamSchema schema;
    if (!catalog.paramSchemaFor(cmd, schema))
        errors.append(QString::fromUtf8(binding->missingSchemaError));
}

void appendModbusScpiValidationErrors(const TestCaseSend& send, QStringList& errors) {
    if (send.device.isEmpty()) {
        errors.append(send.channel == TestCaseSendChannel::Modbus ? QStringLiteral("请选择 Modbus 目标外设")
                                                                  : QStringLiteral("请选择 SCPI 目标外设"));
        return;
    }
    if (send.channel == TestCaseSendChannel::Modbus) {
        const ModbusDeviceRoute devRoute = ModbusDeviceCatalog::deviceRouteFromIni(send.device);
        if (devRoute == ModbusDeviceRoute::None) {
            errors.append(QStringLiteral("Modbus 目标外设无效"));
        } else if (!ModbusPeriphCmdCatalog::isCmdForDevice(devRoute, send.deviceCmd, send.action)) {
            errors.append(QStringLiteral("Modbus 测试指令无效或与操作方式不匹配"));
        }
        return;
    }
    const ScpiDeviceRoute devRoute = ScpiPeriphCmdCatalog::deviceFromIni(send.device);
    if (devRoute == ScpiDeviceRoute::None) {
        errors.append(QStringLiteral("SCPI 目标外设无效"));
    } else if (!ScpiPeriphCmdCatalog::isCmdForDevice(devRoute, send.deviceCmd, send.action)) {
        errors.append(QStringLiteral("SCPI 测试指令无效或与操作方式不匹配"));
    }
}

bool inferChannelFromDeviceCmd(const QString& deviceCmd, TestCaseSendChannel& channel,
                               TestCaseFixtureProtocol& fixtureProtocol) {
    if (tryInferFromCatalog(Asd9026aCmdCatalog::catalog(), deviceCmd, TestCaseSendChannel::Fixture,
                            TestCaseFixtureProtocol::Asd9026a, channel, fixtureProtocol))
        return true;
    if (tryInferFromCatalog(XwdRawFixtureCmdCatalog::catalog(), deviceCmd, TestCaseSendChannel::Fixture,
                            TestCaseFixtureProtocol::Xwd, channel, fixtureProtocol))
        return true;
    if (tryInferFromCatalog(JieliBtBoxCmdCatalog::catalog(), deviceCmd, TestCaseSendChannel::Fixture,
                            TestCaseFixtureProtocol::JieliBtBox, channel, fixtureProtocol))
        return true;
    if (tryInferFromCatalog(FixturePcbaCmdCatalog::catalog(), deviceCmd, TestCaseSendChannel::Fixture,
                            TestCaseFixtureProtocol::Pcba, channel, fixtureProtocol))
        return true;
    if (tryInferFromCatalog(ProductSerialCmdCatalog::catalog(), deviceCmd, TestCaseSendChannel::ProductSerial,
                            TestCaseFixtureProtocol::Pcba, channel, fixtureProtocol))
        return true;
    if (tryInferFromCatalog(TupleCmdCatalog::catalog(), deviceCmd, TestCaseSendChannel::Cloud,
                            TestCaseFixtureProtocol::Pcba, channel, fixtureProtocol))
        return true;
    if (tryInferFromCatalog(DongleCmdCatalog::catalog(), deviceCmd, TestCaseSendChannel::Dongle,
                            TestCaseFixtureProtocol::Pcba, channel, fixtureProtocol))
        return true;
    if (tryInferFromCatalog(UsbCameraCmdCatalog::catalog(), deviceCmd, TestCaseSendChannel::Fixture,
                            TestCaseFixtureProtocol::UsbCamera, channel, fixtureProtocol))
        return true;
    if (tryInferFromCatalog(VesLightCmdCatalog::catalog(), deviceCmd, TestCaseSendChannel::Fixture,
                            TestCaseFixtureProtocol::VesLight, channel, fixtureProtocol))
        return true;
    channel = TestCaseSendChannel::Product;
    fixtureProtocol = TestCaseFixtureProtocol::Pcba;
    return tryInferFromCatalog(DeviceCmdCatalog::catalog(), deviceCmd, TestCaseSendChannel::Product,
                               TestCaseFixtureProtocol::Pcba, channel, fixtureProtocol);
}

void normalizeLegacyUsbCameraSend(TestCaseSend& send) {
    int cmd = 0;
    if (UsbCameraCmdCatalog::catalog().cmdFromName(send.deviceCmd, cmd)
        && send.channel != TestCaseSendChannel::Fixture) {
        send.channel = TestCaseSendChannel::Fixture;
        send.fixtureProtocol = TestCaseFixtureProtocol::UsbCamera;
    }
}

void loadSendParamFromIni(const QSettings& ini, TestCaseSend& send, bool mergeProductHookParamMap) {
    if (send.channel == TestCaseSendChannel::Modbus || send.channel == TestCaseSendChannel::Scpi) {
        loadModbusScpiParamFromIni(ini, send);
        return;
    }
    const CatalogBinding* binding = catalogBinding(send.channel, send.fixtureProtocol);
    if (!binding)
        return;
    int cmd = 0;
    CmdManifestCatalog& catalog = binding->catalogFn();
    if (!catalog.cmdFromName(send.deviceCmd, cmd))
        return;
    if (binding->forceActionFromCatalog) {
        send.action = catalog.actionFor(cmd);
    } else if (!catalog.isCmdForAction(cmd, send.action)) {
        send.action = catalog.actionFor(cmd);
    }
    if (binding->clearParamOnLoad) {
        send.param = QVariant();
    } else {
        catalog.paramFromIniGroup(ini, cmd, send.param);
    }
    if (mergeProductHookParamMap && send.channel == TestCaseSendChannel::Product)
        mergeSendParamMapInto(send.param, readSendParamMap(ini));
}

void writeSendParamToIni(QSettings& ini, const TestCaseSend& send) {
    if (send.channel == TestCaseSendChannel::Modbus || send.channel == TestCaseSendChannel::Scpi) {
        writeScpiModbusParamToIni(ini, send.param);
        return;
    }
    if (send.channel == TestCaseSendChannel::ProductSerial)
        return;
    const CatalogBinding* binding = catalogBinding(send.channel, send.fixtureProtocol);
    if (!binding)
        return;
    int cmd = 0;
    CmdManifestCatalog& catalog = binding->catalogFn();
    if (!catalog.cmdFromName(send.deviceCmd, cmd))
        return;
    catalog.paramToIniGroup(ini, cmd, send.param);
}

} // namespace TestCaseSendDispatch
