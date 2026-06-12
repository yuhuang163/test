#ifndef QSCPIMANAGER_H
#define QSCPIMANAGER_H

#include "huiling_wfp60h_profile.h"
#include "huiling_wfp60h_scpi_device.h"
#include "huiling_wfp60h_scpi_types.h"
#include "iscpi_device.h"
#include "rs_cmw100_scpi_device.h"
#include "rs_cmw100_scpi_types.h"
#include "scpi_types.h"

#include <QObject>
#include <QVariant>
#include <QVariantMap>

class QScpiSerialSession;
class QScpiVisaSession;
class QSerialPort;
class ScpiTransport;

/**
 * SCPI 域工站入口：串口 / VISA 仅驱动不同；各 device 自有 Cmd，经 exec 下发。
 * 设备实现 IScpiDevice 接口，Manager 通过 activeDevice() 多态分发。
 * USB 电流表与 VISA 程控电源/CMW 应使用两个 QScpiManager 实例。
 */
class QScpiManager : public QObject {
    Q_OBJECT

  public:
    explicit QScpiManager(QObject* parent = nullptr);
    explicit QScpiManager(QSerialPort* port, QObject* parent = nullptr);

    void attachSerialPort(QSerialPort* port);
    void setVisaConfig(const ScpiVisaLinkConfig& config);
    ScpiVisaLinkConfig visaConfig() const;

    ScpiLinkKind linkKind() const;
    bool ensureConnected();
    void closeConnection();

    QSerialPort* serialPort() const;
    QScpiSerialSession* session();

    void setDeviceRoute(ScpiDeviceRoute route);
    ScpiDeviceRoute deviceRoute() const;
    bool isScpiDeviceRoute() const;

    void setHuilingProfilePatch(const HuilingWfp60hScpiProfile& patch);
    void clearHuilingProfilePatch();
    HuilingWfp60hScpiProfile huilingActiveProfile() const;

    /** 从 SETTINGS 加载 VISA 会凌程控电源路由与 profile。 */
    void loadHuilingVisaFromSettings();
    /** 从 SETTINGS 加载 VISA CMW100 路由。 */
    void loadCmwVisaFromSettings();

    HuilingWfp60hScpiDevice* huilingDevice();
    RsCmw100ScpiDevice* cmwDevice();

    /** 根据当前 deviceRoute 返回对应的 IScpiDevice 指针（多态分发核心）。 */
    IScpiDevice* activeDevice();

    /** 泛型执行入口：工站可传入任意设备枚举，自动经 IScpiDevice 多态分发。 */
    template <typename CmdType>
    bool exec(CmdType cmd, const QVariant& param = {}, QString* errorMessage = nullptr) {
        IScpiDevice* dev = activeDevice();
        if (!dev) {
            if (errorMessage)
                *errorMessage = QStringLiteral("当前 SCPI 设备路由未配置");
            return false;
        }
        if (!isTransportReady(errorMessage)) {
            return false;
        }
        const int cmdInt = static_cast<int>(cmd);
        const bool ok = dev->isQueryCmd(cmdInt) ? dev->get(cmdInt, param) : dev->set(cmdInt, param);
        if (!ok && errorMessage) {
            *errorMessage = QStringLiteral("SCPI 命令执行失败");
        }
        return ok;
    }

    QString lastQueryResponse() const;

    bool feedRx(const QByteArray& chunk);
    bool sendCustomMessage(const QVariantMap& map);

  signals:
    void measureReadingReceived(const QString& valueText);
    void programmablePowerVoltageRead(double valueVolts, bool ok);
    void programmablePowerCurrentRead(double valueAmps, bool ok);

  private:
    void onLineReceived(const QString& line);
    bool isTransportReady(QString* errorMessage) const;
    void bindActiveTransport(ScpiTransport* transport, ScpiLinkKind kind);

    QScpiSerialSession* serialSession_ = nullptr;
    QScpiVisaSession* visaSession_ = nullptr;
    HuilingWfp60hScpiDevice huilingDevice_;
    RsCmw100ScpiDevice cmwDevice_;
    ScpiTransport* activeTransport_ = nullptr;
    ScpiLinkKind linkKind_ = ScpiLinkKind::Serial;
    ScpiDeviceRoute deviceRoute_ = ScpiDeviceRoute::HuilingWfp60h;
};

#endif // QSCPIMANAGER_H
