#include "xwd_fixture_device.h"

#include "AbIni.h"
#include "fixture_uart_codec.h"
#include "qcoreapplication.h"
#include "qdatetime.h"
#include "qdebug.h"
#include "xwd_amplitude_codec.h"
#include "xwd_line_hex_codec.h"
#include "xwd_line_text_codec.h"

#include <QMessageBox>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

void pulseRelay(XwdFixtureState openState, XwdFixtureState resetState,
                const std::function<void(XwdFixtureState)>& sendState, const std::function<void(int)>& wait) {
    sendState(openState);
    wait(200);
    sendState(resetState);
}

void pulseRelayById(int relayId, const std::function<void(XwdFixtureState)>& sendState,
                    const std::function<void(int)>& wait) {
    if (relayId == 1)
        pulseRelay(XWD_STATE_RELAY1_OPEN, XWD_STATE_RELAY1_RESET, sendState, wait);
    else if (relayId == 2)
        pulseRelay(XWD_STATE_RELAY2_OPEN, XWD_STATE_RELAY2_RESET, sendState, wait);
    else if (relayId == 4)
        pulseRelay(XWD_STATE_RELAY4_OPEN, XWD_STATE_RELAY4_RESET, sendState, wait);
}

void setCylinderStateLineText(int open, int mechine, int currentMechineNo,
                              const std::function<void(XwdFixtureState)>& sendState, const std::function<void(int)>& wait,
                              const std::function<void(int)>& setRelay) {
    if (open) {
        if (mechine == 2) {
            if (currentMechineNo == 3) {
                sendState(XWD_STATE_TRAY_MIDDLE);
                wait(2000);
            }
            sendState(XWD_STATE_RELAY_OPEN);
            wait(50);
            sendState(XWD_STATE_CYLINDER_OPEN);

            const QString productName = SETTINGS.value("Mes/Product_Name").toString();
            if (productName == "Y20PS" || productName == "Y30" || productName == "Y30S")
                wait(5000);
            else
                wait(3000);
            sendState(XWD_STATE_RELAY_RESET);
        } else if (mechine == 1) {
            setRelay(1);
        }
        return;
    }

    sendState(XWD_STATE_CYLINDER_RESET);
    sendState(XWD_STATE_RESET_ALL);
    if (currentMechineNo == 3) {
        wait(2000);
        sendState(XWD_STATE_TRAY_REAR);
    }
}

void setCylinderStateLineHex(int open, int mechine, const std::function<void(XwdFixtureState)>& sendState,
                             const std::function<void(int)>& wait, const std::function<void(int)>& setRelay) {
    if (open) {
        wait(2000);
        sendState(XWD_STATE_CYLINDER_OPEN);
        wait(1500);
        if (mechine == 1)
            setRelay(1);
        else if (mechine == 2)
            setRelay(2);
        return;
    }

    sendState(XWD_STATE_CYLINDER_RESET);
    sendState(XWD_STATE_RESET_ALL);
}

} // namespace

XwdFixtureDevice::XwdFixtureDevice(QSerialPort* parent) : QSerialPort(parent), serialPort(parent) {
}

bool XwdFixtureDevice::isActiveFactory() const {
    return SETTINGS.value(QStringLiteral("Mes/FACTORY"), QStringLiteral("xwd")).toString() == QStringLiteral("xwd");
}

bool XwdFixtureDevice::isLineTextStation(int currentMechine) const {
    return currentMechine == 2 || currentMechine == 3;
}

int XwdFixtureDevice::currentMechineNo() const {
    return SETTINGS.value(QStringLiteral("SYSTEM/CurrentMechine")).toInt();
}

void XwdFixtureDevice::waitWork(int ms) {
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}

QByteArray XwdFixtureDevice::buildTxFrame(XwdFixtureState state) const {
    const int mechineNo = currentMechineNo();
    if (state == XWD_STATE_RELAY4_OPEN || state == XWD_STATE_RELAY4_RESET)
        return XwdLineHexCodec::buildTxFrame(state);
    if (isLineTextStation(mechineNo))
        return XwdLineTextCodec::buildTxFrame(state);
    return XwdLineHexCodec::buildTxFrame(state);
}

void XwdFixtureDevice::writeTxFrame(XwdFixtureState state) {
    if (!serialPort)
        return;
    const QByteArray frame = buildTxFrame(state);
    if (frame.isEmpty())
        return;
    FixtureUartCodec::writeFrame(serialPort, log_, frame);
}

void XwdFixtureDevice::set(FixtureCmd cmd, const QVariant& data) {
    switch (cmd) {
    case XwdFixtureCmd::SendState:
        sendState(static_cast<fixtureState>(data.toInt()));
        return;
    case XwdFixtureCmd::SetCylinderState: {
        const QVariantMap m = data.toMap();
        set_cylinder_state(m.value(QStringLiteral("state")).toInt(),
                           m.value(QStringLiteral("mechine")).toInt());
        return;
    }
    case XwdFixtureCmd::SetRelayState:
        set_relay_state(data.toInt());
        return;
    case XwdFixtureCmd::GetAmplitude:
        get_amplitude();
        return;
    }
}

void XwdFixtureDevice::get(FixtureCmd cmd, const QVariant& param) {
    Q_UNUSED(param);
    if (cmd == XwdFixtureCmd::GetAmplitude)
        get_amplitude();
}

bool XwdFixtureDevice::sendCustomMessage(const QVariantMap& map) {
    const QString action = map.value(QStringLiteral("action")).toString().trimmed().toLower();
    if (action == QStringLiteral("sendstate")) {
        set(XwdFixtureCmd::SendState, map.value(QStringLiteral("state")));
        return true;
    }
    if (action == QStringLiteral("setcylinderstate")) {
        QVariantMap payload;
        payload.insert(QStringLiteral("state"), map.value(QStringLiteral("state")));
        payload.insert(QStringLiteral("mechine"), map.value(QStringLiteral("mechine")));
        set(XwdFixtureCmd::SetCylinderState, payload);
        return true;
    }
    if (action == QStringLiteral("setrelaystate")) {
        set(XwdFixtureCmd::SetRelayState, map.value(QStringLiteral("state")));
        return true;
    }
    if (action == QStringLiteral("getamplitude")) {
        get(XwdFixtureCmd::GetAmplitude);
        return true;
    }
    return false;
}

void XwdFixtureDevice::set_relay_state(int state) {
    if (!isActiveFactory())
        return;

    const auto sendFn = [this](XwdFixtureState s) { writeTxFrame(s); };
    const auto waitFn = [this](int ms) { waitWork(ms); };
    pulseRelayById(state, sendFn, waitFn);
}

void XwdFixtureDevice::set_cylinder_state(int state, int mechine) {
    if (!isActiveFactory())
        return;

    const int mechineNo = currentMechineNo();
    const auto sendFn = [this](XwdFixtureState s) { writeTxFrame(s); };
    const auto waitFn = [this](int ms) { waitWork(ms); };
    const auto relayFn = [this](int relayId) { set_relay_state(relayId); };

    if (isLineTextStation(mechineNo)) {
        setCylinderStateLineText(state, mechine, mechineNo, sendFn, waitFn, relayFn);
        return;
    }
    setCylinderStateLineHex(state, mechine, sendFn, waitFn, relayFn);
}

void XwdFixtureDevice::sendState(fixtureState fixstate) {
    if (!isActiveFactory())
        return;
    if (!serialPort || !serialPort->isOpen()) {
        QMessageBox::warning(NULL, "警告", " 未打开治具串口\t\r\n  无法发送数据！\r\n");
        return;
    }
    writeTxFrame(static_cast<XwdFixtureState>(fixstate));
}

void XwdFixtureDevice::parseCmd(const QByteArray& byte) {
    if (!isActiveFactory())
        return;

    int peakAmplitude = 0;
    if (!XwdAmplitudeCodec::parsePeakAmplitude(byte, &peakAmplitude))
        return;

    const int errorValue = SETTINGS.value("Pressure/AmplitudeError", 0).toInt();
    const int result = (peakAmplitude + errorValue) * 6;
    qDebug() << "发送结果，摆幅:" << peakAmplitude << "误差值:" << errorValue;
    emitAmplitudeReport(QString::number(result));
}

void XwdFixtureDevice::emitAmplitudeReport(const QString& resultText) {
    emitReport(QStringLiteral("ProtocolJigAmplitudeData"),
               QVariant::fromValue(ProtocolJigAmplitudeData{resultText}));
}

void XwdFixtureDevice::get_amplitude() {
    if (!isActiveFactory() || !serialPort)
        return;
    FixtureUartCodec::writeFrame(serialPort, log_, FixtureUartCodec::amplitudeQueryCommand());
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
