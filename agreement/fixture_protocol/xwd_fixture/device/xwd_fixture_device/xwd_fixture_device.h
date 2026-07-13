#ifndef XWD_FIXTURE_DEVICE_H
#define XWD_FIXTURE_DEVICE_H

#include <QSerialPort>
#include <QVariant>
#include <QVariantMap>

#include "qlog.h"
#include "qprotocol_types.h"
#include "xwd_fixture_types.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达 xwd 治具设备：按机台路由文本/十六进制协议。 */
class XwdFixtureDevice : public QSerialPort {
    Q_OBJECT
  public:
    using FixtureCmd = ::XwdFixtureCmd;

    explicit XwdFixtureDevice(QSerialPort* parent = nullptr);

    QSerialPort* serialPort = nullptr;

    enum fixtureState {
        STATE_CYLINDER_OPEN = XWD_STATE_CYLINDER_OPEN,
        STATE_RELAY_OPEN = XWD_STATE_RELAY_OPEN,
        STATE_RELAY_RESET = XWD_STATE_RELAY_RESET,
        STATE_RELAY1_OPEN = XWD_STATE_RELAY1_OPEN,
        STATE_RELAY1_RESET = XWD_STATE_RELAY1_RESET,
        STATE_RELAY2_OPEN = XWD_STATE_RELAY2_OPEN,
        STATE_RELAY2_RESET = XWD_STATE_RELAY2_RESET,
        STATE_CYLINDER_RESET = XWD_STATE_CYLINDER_RESET,
        STATE_RELAY4_OPEN = XWD_STATE_RELAY4_OPEN,
        STATE_RELAY4_RESET = XWD_STATE_RELAY4_RESET,
        STATE_RESET_ALL = XWD_STATE_RESET_ALL,
        STATE_PEN_PRESS = XWD_STATE_PEN_PRESS,
        STATE_TRAY_MIDDLE = XWD_STATE_TRAY_MIDDLE,
        STATE_TRAY_REAR = XWD_STATE_TRAY_REAR,
        STATE_FRONT = XWD_STATE_FRONT,
    };

    void sendState(fixtureState fixstate);
    void set_cylinder_state(int state, int mechine);
    void waitWork(int ms);
    void set_relay_state(int state);
    void get_amplitude();
    void parseCmd(const QByteArray& byte);
    void set(FixtureCmd cmd, const QVariant& data = {});
    void get(FixtureCmd cmd, const QVariant& param = {});
    bool sendCustomMessage(const QVariantMap& map);

  signals:
    void reportReceived(const ProtocolReport& report);

  private:
    bool isActiveFactory() const;
    bool isLineTextStation(int currentMechine) const;
    int currentMechineNo() const;
    QByteArray buildTxFrame(XwdFixtureState state) const;
    void writeTxFrame(XwdFixtureState state);
    void emitAmplitudeReport(const QString& resultText);

    void emitReport(const QString& reportType, const QVariant& payload = QVariant()) {
        emit reportReceived(ProtocolReport(reportType, payload));
    }

    Qlog log_;
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // XWD_FIXTURE_DEVICE_H
