#include "test_case.h"

#include "asd9026a_cmd_manifest.h"
#include "cmd_catalog_base.h"
#include "device_cmd_manifest.h"
#include "dongle_cmd_manifest.h"
#include "fixture_pcba_cmd_manifest.h"
#include "jieli_bt_box_cmd_manifest.h"
#include "modbus_cmd_manifest.h"
#include "product_serial_cmd_manifest.h"
#include "scpi_cmd_manifest.h"
#include "test_case_cmd_catalogs.h"
#include "test_case_ini_param.h"
#include "tuple_cmd_manifest.h"
#include "usb_camera_cmd_manifest.h"
#include "ves_light_cmd_manifest.h"
#include "xwd_fixture_cmd_manifest.h"

#include <QMetaType>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// ---------- manifest 表 → CmdManifestRegistry ----------

static CmdManifestRegistry makeDeviceCmdRegistry(const CmdManifestRegistry::Policy& policy) {
    CmdManifestRegistry reg;
    reg.policy = policy;
    reg.rows.reserve(DeviceCmdManifest::rowCount());
    for (int i = 0; i < DeviceCmdManifest::rowCount(); ++i) {
        const DeviceCmdManifest::Row& r = DeviceCmdManifest::rows()[i];
        reg.rows.append({static_cast<int>(r.cmd), r.enumName, r.uiLabel, r.paramKind, r.paramHint, r.sendActions});
    }
    return reg;
}

static CmdManifestRegistry makeDongleCmdRegistry(const CmdManifestRegistry::Policy& policy) {
    CmdManifestRegistry reg;
    reg.policy = policy;
    reg.rows.reserve(DongleCmdManifest::rowCount());
    for (int i = 0; i < DongleCmdManifest::rowCount(); ++i) {
        const DongleCmdManifest::Row& r = DongleCmdManifest::rows()[i];
        reg.rows.append({static_cast<int>(r.cmd), r.enumName, r.uiLabel, r.paramKind, r.paramHint, r.sendActions});
    }
    return reg;
}

static CmdManifestRegistry makeTupleCmdRegistry(const CmdManifestRegistry::Policy& policy) {
    CmdManifestRegistry reg;
    reg.policy = policy;
    reg.rows.reserve(TupleCmdManifest::rowCount());
    for (int i = 0; i < TupleCmdManifest::rowCount(); ++i) {
        const TupleCmdManifest::Row& r = TupleCmdManifest::rows()[i];
        reg.rows.append({static_cast<int>(r.cmd), r.enumName, r.uiLabel, r.paramKind, r.paramHint, r.sendActions});
    }
    return reg;
}

static CmdManifestRegistry makeProductSerialRegistry(const CmdManifestRegistry::Policy& policy) {
    CmdManifestRegistry reg;
    reg.policy = policy;
    reg.rows.reserve(ProductSerialCmdManifest::rowCount());
    for (int i = 0; i < ProductSerialCmdManifest::rowCount(); ++i) {
        const ProductSerialCmdManifest::Row& r = ProductSerialCmdManifest::rows()[i];
        CmdManifestRegistry::Row row;
        row.cmd = static_cast<int>(r.cmd);
        row.enumName = r.enumName;
        row.uiLabel = r.uiLabel;
        row.paramKind = DeviceCmdParamKind::None;
        row.paramHint = r.paramHint;
        row.sendActions = r.sendActions;
        reg.rows.append(row);
    }
    return reg;
}

static CmdManifestRegistry makeAsd9026aCmdRegistry(const CmdManifestRegistry::Policy& policy) {
    CmdManifestRegistry reg;
    reg.policy = policy;
    reg.rows.reserve(Asd9026aCmdManifest::rowCount());
    for (int i = 0; i < Asd9026aCmdManifest::rowCount(); ++i) {
        const Asd9026aCmdManifest::Row& r = Asd9026aCmdManifest::rows()[i];
        reg.rows.append({static_cast<int>(r.cmd), r.enumName, r.uiLabel, r.paramKind, r.paramHint, r.sendActions});
    }
    return reg;
}

static CmdManifestRegistry makeXwdRawFixtureCmdRegistry(const CmdManifestRegistry::Policy& policy) {
    CmdManifestRegistry reg;
    reg.policy = policy;
    reg.rows.reserve(XwdRawFixtureCmdManifest::rowCount());
    for (int i = 0; i < XwdRawFixtureCmdManifest::rowCount(); ++i) {
        const XwdRawFixtureCmdManifest::Row& r = XwdRawFixtureCmdManifest::rows()[i];
        reg.rows.append({static_cast<int>(r.cmd), r.enumName, r.uiLabel, r.paramKind, r.paramHint, r.sendActions});
    }
    return reg;
}

static CmdManifestRegistry makeJieliBtBoxCmdRegistry(const CmdManifestRegistry::Policy& policy) {
    CmdManifestRegistry reg;
    reg.policy = policy;
    reg.rows.reserve(JieliBtBoxCmdManifest::rowCount());
    for (int i = 0; i < JieliBtBoxCmdManifest::rowCount(); ++i) {
        const JieliBtBoxCmdManifest::Row& r = JieliBtBoxCmdManifest::rows()[i];
        reg.rows.append({static_cast<int>(r.cmd), r.enumName, r.uiLabel, r.paramKind, r.paramHint, r.sendActions});
    }
    return reg;
}

static CmdManifestRegistry makeUsbCameraCmdRegistry(const CmdManifestRegistry::Policy& policy) {
    CmdManifestRegistry reg;
    reg.policy = policy;
    reg.rows.reserve(UsbCameraCmdManifest::rowCount());
    for (int i = 0; i < UsbCameraCmdManifest::rowCount(); ++i) {
        const UsbCameraCmdManifest::Row& r = UsbCameraCmdManifest::rows()[i];
        reg.rows.append({static_cast<int>(r.cmd), r.enumName, r.uiLabel, r.paramKind, r.paramHint, r.sendActions});
    }
    return reg;
}

static CmdManifestRegistry makeVesLightCmdRegistry(const CmdManifestRegistry::Policy& policy) {
    CmdManifestRegistry reg;
    reg.policy = policy;
    reg.rows.reserve(VesLightCmdManifest::rowCount());
    for (int i = 0; i < VesLightCmdManifest::rowCount(); ++i) {
        const VesLightCmdManifest::Row& r = VesLightCmdManifest::rows()[i];
        reg.rows.append({static_cast<int>(r.cmd), r.enumName, r.uiLabel, r.paramKind, r.paramHint, r.sendActions});
    }
    return reg;
}

static CmdManifestRegistry makeFixturePcbaCmdRegistry(const CmdManifestRegistry::Policy& policy) {
    CmdManifestRegistry reg;
    reg.policy = policy;
    reg.rows.reserve(FixturePcbaCmdManifest::rowCount());
    for (int i = 0; i < FixturePcbaCmdManifest::rowCount(); ++i) {
        const FixturePcbaCmdManifest::Row& r = FixturePcbaCmdManifest::rows()[i];
        reg.rows.append({static_cast<int>(r.cmd), r.enumName, r.uiLabel, r.paramKind, r.paramHint, r.sendActions});
    }
    return reg;
}

// ---------- DeviceCmdManifestCatalog ----------

CmdManifestRegistry::Policy DeviceCmdManifestCatalog::devicePolicy() {
    CmdManifestRegistry::Policy p;
    p.unknownCmdLabel = QStringLiteral("未登记指令");
    p.unknownNameParamHint = QStringLiteral("未知产品指令");
    p.unregisteredParamHint = QStringLiteral("按协议填写 JSON 或 name=value 行");
    p.paramIniProfile = CmdCatalogParamIniProfile::WithUIntAndLegacyJson;
    return p;
}

DeviceCmdManifestCatalog::DeviceCmdManifestCatalog()
    : CmdManifestCatalog(makeDeviceCmdRegistry(devicePolicy())) {}

void DeviceCmdManifestCatalog::paramToIniGroup(QSettings& settings, int cmd, const QVariant& value) const {
    removeSendParamKeys(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    const QString prefix = sendParamIniPrefix();
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        break;
    case DeviceCmdParamKind::Int:
        writeSendParamLeaf(settings, QStringLiteral("int"), value.toInt());
        break;
    case DeviceCmdParamKind::UInt:
        writeSendParamLeaf(settings, QStringLiteral("uint"), value.toUInt());
        break;
    case DeviceCmdParamKind::String:
        writeSendParamLeaf(settings, QStringLiteral("string"), value.toString());
        break;
    case DeviceCmdParamKind::JsonMap:
        if (value.canConvert<DeviceSnPayload>()) {
            const DeviceSnPayload payload = value.value<DeviceSnPayload>();
            QVariantMap map;
            map.insert(QStringLiteral("which_sn"), static_cast<int>(payload.which_sn));
            map.insert(QStringLiteral("sn"), QString::fromUtf8(payload.sn));
            if (payload.sideId >= 0 && payload.sideId <= 2)
                map.insert(QStringLiteral("side"), payload.sideId);
            writeJsonMap(settings, prefix, map);
        } else if (value.canConvert<QVariantMap>()) {
            writeJsonMap(settings, prefix, value);
        } else if (value.type() == QVariant::String) {
            writeSendParamLeaf(settings, QStringLiteral("value"), value.toString());
        } else {
            writeSendParamLeaf(settings, QStringLiteral("value"), value.toInt());
        }
        break;
    }
}

// ---------- DongleCmdManifestCatalog ----------

CmdManifestRegistry::Policy DongleCmdManifestCatalog::policy() {
    CmdManifestRegistry::Policy p;
    p.unknownCmdLabel = QStringLiteral("未登记 Dongle 指令");
    p.unknownNameParamHint = QStringLiteral("未知 Dongle 指令");
    p.unregisteredParamHint = QStringLiteral("该 Dongle 指令未登记");
    return p;
}

DongleCmdManifestCatalog::DongleCmdManifestCatalog()
    : CmdManifestCatalog(makeDongleCmdRegistry(policy())) {}

// ---------- TupleCmdManifestCatalog ----------

CmdManifestRegistry::Policy TupleCmdManifestCatalog::policy() {
    CmdManifestRegistry::Policy p;
    p.unknownCmdLabel = QStringLiteral("未登记云端指令");
    p.unknownNameParamHint = QStringLiteral("未知云端指令");
    p.unregisteredParamHint = QStringLiteral("该云端指令未登记");
    return p;
}

TupleCmdManifestCatalog::TupleCmdManifestCatalog()
    : CmdManifestCatalog(makeTupleCmdRegistry(policy())) {}

// ---------- ProductSerialCmdManifestCatalog ----------

CmdManifestRegistry::Policy ProductSerialCmdManifestCatalog::policy() {
    CmdManifestRegistry::Policy p;
    p.unknownCmdLabel = QStringLiteral("未登记串口指令");
    p.cmdToNameFallback = CmdCatalogCmdToNameFallback::Empty;
    p.paramHintStyle = CmdCatalogParamHintStyle::HintOnly;
    p.filterBySendAction = false;
    p.paramIniProfile = CmdCatalogParamIniProfile::None;
    return p;
}

ProductSerialCmdManifestCatalog::ProductSerialCmdManifestCatalog()
    : CmdManifestCatalog(makeProductSerialRegistry(policy())) {}

// ---------- Asd9026aCmdManifestCatalog ----------

CmdManifestRegistry::Policy Asd9026aCmdManifestCatalog::asd9026aPolicy() {
    CmdManifestRegistry::Policy p;
    p.uiLabelMode = CmdCatalogUiLabelMode::Raw;
    p.cmdToNameFallback = CmdCatalogCmdToNameFallback::Empty;
    p.paramHintStyle = CmdCatalogParamHintStyle::HintOnly;
    return p;
}

Asd9026aCmdManifestCatalog::Asd9026aCmdManifestCatalog()
    : CmdManifestCatalog(makeAsd9026aCmdRegistry(asd9026aPolicy())) {}

// ---------- XwdRawFixtureCmdManifestCatalog ----------

CmdManifestRegistry::Policy XwdRawFixtureCmdManifestCatalog::xwdPolicy() {
    CmdManifestRegistry::Policy p;
    p.uiLabelMode = CmdCatalogUiLabelMode::Raw;
    p.cmdToNameFallback = CmdCatalogCmdToNameFallback::Empty;
    p.paramHintStyle = CmdCatalogParamHintStyle::HintOnly;
    return p;
}

XwdRawFixtureCmdManifestCatalog::XwdRawFixtureCmdManifestCatalog()
    : CmdManifestCatalog(makeXwdRawFixtureCmdRegistry(xwdPolicy())) {}

// ---------- JieliBtBoxCmdManifestCatalog ----------

CmdManifestRegistry::Policy JieliBtBoxCmdManifestCatalog::jieliPolicy() {
    CmdManifestRegistry::Policy p;
    p.missingCmdDefaultAction = TestCaseSendAction::Get;
    p.uiLabelMode = CmdCatalogUiLabelMode::Raw;
    p.cmdToNameFallback = CmdCatalogCmdToNameFallback::Empty;
    p.paramHintStyle = CmdCatalogParamHintStyle::HintOnly;
    return p;
}

JieliBtBoxCmdManifestCatalog::JieliBtBoxCmdManifestCatalog()
    : CmdManifestCatalog(makeJieliBtBoxCmdRegistry(jieliPolicy())) {}

// ---------- UsbCameraCmdManifestCatalog ----------

CmdManifestRegistry::Policy UsbCameraCmdManifestCatalog::usbCameraPolicy() {
    CmdManifestRegistry::Policy p;
    p.unknownCmdLabel = QStringLiteral("未登记 USB 摄像头指令");
    p.unknownNameParamHint = QStringLiteral("未知 USB 摄像头指令");
    p.unregisteredParamHint = QStringLiteral("该 USB 摄像头指令未登记");
    p.missingCmdDefaultAction = TestCaseSendAction::Get;
    p.paramIniProfile = CmdCatalogParamIniProfile::Custom;
    return p;
}

UsbCameraCmdManifestCatalog::UsbCameraCmdManifestCatalog()
    : CmdManifestCatalog(makeUsbCameraCmdRegistry(usbCameraPolicy())) {}

bool UsbCameraCmdManifestCatalog::paramFromIniGroup(const QSettings& settings, int cmd, QVariant& out) const {
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return false;
    switch (schema.kind) {
    case DeviceCmdParamKind::None:
        out = QVariant();
        return true;
    case DeviceCmdParamKind::JsonMap: {
        QVariantMap map = readSendParamMap(settings);
        map.remove(QStringLiteral("setScreenColor"));
        out = map;
        return true;
    }
    default:
        return false;
    }
}

void UsbCameraCmdManifestCatalog::paramToIniGroup(QSettings& settings, int cmd, const QVariant& value) const {
    removeSendParamKeys(settings);
    DeviceCmdParamSchema schema;
    if (!paramSchemaFor(cmd, schema))
        return;
    if (schema.kind != DeviceCmdParamKind::JsonMap)
        return;
    QVariant cleaned = value;
    if (cleaned.canConvert<QVariantMap>()) {
        QVariantMap map = cleaned.toMap();
        map.remove(QStringLiteral("setScreenColor"));
        cleaned = map;
    }
    writeJsonMap(settings, sendParamIniPrefix(), cleaned);
}

// ---------- VesLightCmdManifestCatalog ----------

CmdManifestRegistry::Policy VesLightCmdManifestCatalog::vesLightPolicy() {
    CmdManifestRegistry::Policy p;
    p.unknownCmdLabel = QStringLiteral("未登记 VES 光源指令");
    p.unknownNameParamHint = QStringLiteral("未知 VES 光源指令");
    p.paramHintStyle = CmdCatalogParamHintStyle::HintOnly;
    return p;
}

VesLightCmdManifestCatalog::VesLightCmdManifestCatalog()
    : CmdManifestCatalog(makeVesLightCmdRegistry(vesLightPolicy())) {}

// ---------- FixturePcbaCmdManifestCatalog ----------

CmdManifestRegistry::Policy FixturePcbaCmdManifestCatalog::fixturePcbaPolicy() {
    CmdManifestRegistry::Policy p;
    p.unknownCmdLabel = QStringLiteral("未登记治具指令");
    p.cmdToNameFallback = CmdCatalogCmdToNameFallback::Empty;
    p.paramHintStyle = CmdCatalogParamHintStyle::HintOnly;
    p.paramIniProfile = CmdCatalogParamIniProfile::Custom;
    return p;
}

FixturePcbaCmdManifestCatalog::FixturePcbaCmdManifestCatalog()
    : CmdManifestCatalog(makeFixturePcbaCmdRegistry(fixturePcbaPolicy())) {}

bool FixturePcbaCmdManifestCatalog::paramFromIniGroup(const QSettings& settings, int cmd, QVariant& out) const {
    switch (static_cast<FixturePcbaCmd>(cmd)) {
    case FixturePcbaCmd::StartTest:
    case FixturePcbaCmd::StartSleep:
    case FixturePcbaCmd::StartWhiteMode: {
        QVariant machine = readSendScopedParam(settings, QStringLiteral("MachineIndex"), QVariant());
        if (!machine.isValid())
            machine = settings.value(sendParamIniKey(QStringLiteral("MachineIndex")));
        if (!machine.isValid()) {
            const QString legacyKey = QStringLiteral("SendParam/MachineIndex");
            if (settings.contains(legacyKey))
                machine = settings.value(legacyKey);
        }
        if (!machine.isValid()) {
            out = QStringLiteral("$INDEX");
            break;
        }
        if (machine.userType() == QMetaType::QString) {
            const QString s = machine.toString().trimmed();
            if (s.compare(QStringLiteral("$INDEX"), Qt::CaseInsensitive) == 0
                || s.compare(QStringLiteral("$SLOT"), Qt::CaseInsensitive) == 0 || s.isEmpty()) {
                out = QStringLiteral("$INDEX");
                break;
            }
        }
        bool ok = false;
        int idx = machine.toInt(&ok);
        if (!ok || idx == 0)
            out = QStringLiteral("$INDEX");
        else
            out = qBound(1, idx, 15);
        break;
    }
    default:
        out = QVariant();
        break;
    }
    return true;
}

void FixturePcbaCmdManifestCatalog::paramToIniGroup(QSettings& settings, int cmd, const QVariant& value) const {
    removeKeysWithPrefix(settings, QStringLiteral("SendParam"));
    removeSendParamKeys(settings);
    switch (static_cast<FixturePcbaCmd>(cmd)) {
    case FixturePcbaCmd::StartTest:
    case FixturePcbaCmd::StartSleep:
    case FixturePcbaCmd::StartWhiteMode:
        if (value.userType() == QMetaType::QString)
            writeSendParamLeaf(settings, QStringLiteral("MachineIndex"), value.toString().trimmed());
        else
            writeSendParamLeaf(settings, QStringLiteral("MachineIndex"), value.toInt());
        break;
    default:
        break;
    }
}

// ---------- DeviceCmdCatalog ----------

CmdManifestCatalog& DeviceCmdCatalog::catalog() {
    static DeviceCmdManifestCatalog catalog;
    return catalog;
}

TestCaseProductProtocol DeviceCmdCatalog::productProtocolFromIni(const QString& text) {
    const QString t = text.trimmed();
    if (t.compare(QStringLiteral("Qpb"), Qt::CaseInsensitive) == 0 || t.compare(QStringLiteral("PB"), Qt::CaseInsensitive) == 0)
        return TestCaseProductProtocol::Qpb;
    if (t.compare(QStringLiteral("Qroot"), Qt::CaseInsensitive) == 0)
        return TestCaseProductProtocol::Qroot;
    if (t.compare(QStringLiteral("Qaiot"), Qt::CaseInsensitive) == 0)
        return TestCaseProductProtocol::Qaiot;
    return TestCaseProductProtocol::Qfctp;
}

QString DeviceCmdCatalog::productProtocolToIni(TestCaseProductProtocol protocol) {
    switch (protocol) {
    case TestCaseProductProtocol::Qpb:
        return QStringLiteral("Qpb");
    case TestCaseProductProtocol::Qroot:
        return QStringLiteral("Qroot");
    case TestCaseProductProtocol::Qaiot:
        return QStringLiteral("Qaiot");
    default:
        return QStringLiteral("Qfctp");
    }
}

QString DeviceCmdCatalog::productProtocolUiLabel(TestCaseProductProtocol protocol) {
    switch (protocol) {
    case TestCaseProductProtocol::Qpb:
        return QStringLiteral("QPB");
    case TestCaseProductProtocol::Qroot:
        return QStringLiteral("Qroot");
    case TestCaseProductProtocol::Qaiot:
        return QStringLiteral("QAIOT");
    default:
        return QStringLiteral("FCTP");
    }
}

// ---------- DongleCmdCatalog / TupleCmdCatalog / ProductSerialCmdCatalog ----------

CmdManifestCatalog& DongleCmdCatalog::catalog() {
    static DongleCmdManifestCatalog catalog;
    return catalog;
}

CmdManifestCatalog& TupleCmdCatalog::catalog() {
    static TupleCmdManifestCatalog catalog;
    return catalog;
}

CmdManifestCatalog& ProductSerialCmdCatalog::catalog() {
    static ProductSerialCmdManifestCatalog catalog;
    return catalog;
}

// ---------- 治具 Fixture 各协议 Catalog ----------

CmdManifestCatalog& Asd9026aCmdCatalog::catalog() {
    static Asd9026aCmdManifestCatalog catalog;
    return catalog;
}

CmdManifestCatalog& XwdRawFixtureCmdCatalog::catalog() {
    static XwdRawFixtureCmdManifestCatalog catalog;
    return catalog;
}

CmdManifestCatalog& JieliBtBoxCmdCatalog::catalog() {
    static JieliBtBoxCmdManifestCatalog catalog;
    return catalog;
}

CmdManifestCatalog& UsbCameraCmdCatalog::catalog() {
    static UsbCameraCmdManifestCatalog catalog;
    return catalog;
}

CmdManifestCatalog& VesLightCmdCatalog::catalog() {
    static VesLightCmdManifestCatalog catalog;
    return catalog;
}

CmdManifestCatalog& FixturePcbaCmdCatalog::catalog() {
    static FixturePcbaCmdManifestCatalog catalog;
    return catalog;
}

TestCaseFixtureProtocol FixturePcbaCmdCatalog::fixtureProtocolFromIni(const QString& text) {
    if (text.compare(QStringLiteral("ASD9026A"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::Asd9026a;
    if (text.compare(QStringLiteral("XWD"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("Xwd"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("XWD_BLE"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("XwdBle"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("XWD_SUCTION"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("XwdSuction"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::Xwd;
    if (text.compare(QStringLiteral("JIELI_BT_BOX"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("JieliBtBox"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::JieliBtBox;
    if (text.compare(QStringLiteral("USB_CAMERA"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("UsbCamera"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::UsbCamera;
    if (text.compare(QStringLiteral("VES"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("VES_LIGHT"), Qt::CaseInsensitive) == 0
        || text.compare(QStringLiteral("VesLight"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::VesLight;
    if (text.compare(QStringLiteral("Pcba"), Qt::CaseInsensitive) == 0 || text.compare(QStringLiteral("PCBA"), Qt::CaseInsensitive) == 0)
        return TestCaseFixtureProtocol::Pcba;
    return TestCaseFixtureProtocol::Pcba;
}

QString FixturePcbaCmdCatalog::fixtureProtocolToIni(TestCaseFixtureProtocol protocol) {
    switch (protocol) {
    case TestCaseFixtureProtocol::Asd9026a:
        return QStringLiteral("ASD9026A");
    case TestCaseFixtureProtocol::Xwd:
        return QStringLiteral("XWD");
    case TestCaseFixtureProtocol::JieliBtBox:
        return QStringLiteral("JIELI_BT_BOX");
    case TestCaseFixtureProtocol::UsbCamera:
        return QStringLiteral("USB_CAMERA");
    case TestCaseFixtureProtocol::VesLight:
        return QStringLiteral("VES");
    case TestCaseFixtureProtocol::Pcba:
    default:
        return QStringLiteral("Pcba");
    }
}

QString FixturePcbaCmdCatalog::fixtureProtocolUiLabel(TestCaseFixtureProtocol protocol) {
    switch (protocol) {
    case TestCaseFixtureProtocol::Asd9026a:
        return QStringLiteral("ASD9026A模拟电池");
    case TestCaseFixtureProtocol::Xwd:
        return QStringLiteral("XWD治具");
    case TestCaseFixtureProtocol::JieliBtBox:
        return QStringLiteral("杰理蓝牙盒子");
    case TestCaseFixtureProtocol::UsbCamera:
        return QStringLiteral("USB摄像头");
    case TestCaseFixtureProtocol::VesLight:
        return QStringLiteral("VES光源");
    case TestCaseFixtureProtocol::Pcba:
    default:
        return QStringLiteral("PCBA测试协议");
    }
}

// ---------- ModbusPeriphCmdCatalog ----------

QStringList ModbusPeriphCmdCatalog::allDeviceKeys() {
    return {ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::InovanceH5uTcp),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::GcSeriesTcp),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::HqAmmeterRtu),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::LxAmmeterRtu),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::MultiTempLoggerRtu),
            ModbusDeviceCatalog::deviceRouteToIni(ModbusDeviceRoute::XinjiePlcRtu)};
}

QStringList ModbusPeriphCmdCatalog::allCmdNames(ModbusDeviceRoute device, TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < ModbusCmdManifest::rowCount(); ++i) {
        const ModbusCmdManifest::Row& row = ModbusCmdManifest::rows()[i];
        if (row.device != device) {
            continue;
        }
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action)) {
            continue;
        }
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

bool ModbusPeriphCmdCatalog::isCmdForDevice(ModbusDeviceRoute device, const QString& enumName,
                                            TestCaseSendAction action) {
    const ModbusCmdManifest::Row* row = ModbusCmdManifest::findByDeviceAndName(device, enumName);
    if (!row) {
        return false;
    }
    return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
}

QString ModbusPeriphCmdCatalog::cmdUiLabel(ModbusDeviceRoute device, const QString& enumName) {
    const ModbusCmdManifest::Row* row = ModbusCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->uiLabel ? QString::fromUtf8(row->uiLabel) : enumName;
}

QString ModbusPeriphCmdCatalog::paramUiHint(ModbusDeviceRoute device, const QString& enumName) {
    const ModbusCmdManifest::Row* row = ModbusCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->paramHint ? QString::fromUtf8(row->paramHint) : QString();
}

// ---------- ScpiPeriphCmdCatalog ----------

QStringList ScpiPeriphCmdCatalog::allDeviceKeys() {
    return {QStringLiteral("HuilingWfp60h"), QStringLiteral("Agilent66319d"), QStringLiteral("RsCmw100")};
}

QString ScpiPeriphCmdCatalog::deviceUiLabel(ScpiDeviceRoute device) {
    switch (device) {
    case ScpiDeviceRoute::HuilingWfp60h:
        return QStringLiteral("WFP60H 程控电源");
    case ScpiDeviceRoute::Agilent66319d:
        return QStringLiteral("66319D程控电源");
    case ScpiDeviceRoute::RsCmw100:
        return QStringLiteral("罗德与施瓦茨 CMW100");
    default:
        return QStringLiteral("未知设备");
    }
}

ScpiDeviceRoute ScpiPeriphCmdCatalog::deviceFromIni(const QString& text) {
    const QString t = text.trimmed();
    if (t.compare(QStringLiteral("HuilingWfp60h"), Qt::CaseInsensitive) == 0)
        return ScpiDeviceRoute::HuilingWfp60h;
    if (t.compare(QStringLiteral("Agilent66319d"), Qt::CaseInsensitive) == 0)
        return ScpiDeviceRoute::Agilent66319d;
    if (t.compare(QStringLiteral("RsCmw100"), Qt::CaseInsensitive) == 0)
        return ScpiDeviceRoute::RsCmw100;
    return ScpiDeviceRoute::None;
}

QString ScpiPeriphCmdCatalog::deviceToIni(ScpiDeviceRoute device) {
    switch (device) {
    case ScpiDeviceRoute::HuilingWfp60h:
        return QStringLiteral("HuilingWfp60h");
    case ScpiDeviceRoute::Agilent66319d:
        return QStringLiteral("Agilent66319d");
    case ScpiDeviceRoute::RsCmw100:
        return QStringLiteral("RsCmw100");
    default:
        return QStringLiteral("None");
    }
}

QStringList ScpiPeriphCmdCatalog::allCmdNames(ScpiDeviceRoute device, TestCaseSendAction action) {
    QStringList names;
    for (int i = 0; i < ScpiCmdManifest::rowCount(); ++i) {
        const ScpiCmdManifest::Row& row = ScpiCmdManifest::rows()[i];
        if (row.device != device) {
            continue;
        }
        if (!TestCaseCmdManifest::matchesSendAction(row.sendActions, action)) {
            continue;
        }
        names.append(QString::fromLatin1(row.enumName));
    }
    names.sort();
    return names;
}

bool ScpiPeriphCmdCatalog::isCmdForDevice(ScpiDeviceRoute device, const QString& enumName,
                                          TestCaseSendAction action) {
    const ScpiCmdManifest::Row* row = ScpiCmdManifest::findByDeviceAndName(device, enumName);
    if (!row) {
        return false;
    }
    return TestCaseCmdManifest::matchesSendAction(row->sendActions, action);
}

QString ScpiPeriphCmdCatalog::cmdUiLabel(ScpiDeviceRoute device, const QString& enumName) {
    const ScpiCmdManifest::Row* row = ScpiCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->uiLabel ? QString::fromUtf8(row->uiLabel) : enumName;
}

QString ScpiPeriphCmdCatalog::paramUiHint(ScpiDeviceRoute device, const QString& enumName) {
    const ScpiCmdManifest::Row* row = ScpiCmdManifest::findByDeviceAndName(device, enumName);
    return row && row->paramHint ? QString::fromUtf8(row->paramHint) : QString();
}
