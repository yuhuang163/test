#ifndef DONGLE_AT_DEVICE_H
#define DONGLE_AT_DEVICE_H

#include <QObject>
#include <QVariant>
#include <QVariantMap>
#include <functional>
#include <map>
#include "dongle_at_types.h"
#include "../../codec/at_line_codec.h"
#include "qprotocol_types.h"
 
class DongleAtDevice : public QObject {
    Q_OBJECT
  public:
    explicit DongleAtDevice(QObject* parent = nullptr);

  // clang-format off
    using WriteCallback = std::function<void(const QByteArray& data)>;
    void setWriteCallback(const WriteCallback& cb) { writeCb_ = cb; }
    void onFrameReceived(const AtFrame& frame);
    void set(DongleCmd cmd, const QVariant& data = {});
    void get(DongleCmd cmd, const QVariant& param = {});
    bool sendCustomMessage(const QVariantMap& map);
    bool getConnected() const { return isConnected; }
    void resetConnected() { isConnected = false; }
    void setConnected() { isConnected = true; }
    bool getwifiConnected() const { return iswifiConnected; }
    void resetwifiConnected() { iswifiConnected = false; }
    void setwifiConnected() { iswifiConnected = true; }
    // clang-format on

  signals:
    void reportReceived(const ProtocolReport& report);
    void sendGetProductResponse(int data);

  protected:
    void emitReport(const QString& reportType, const QVariant& payload = QVariant()) {
        emit reportReceived(ProtocolReport(reportType, payload));
    }

  private:
    void registerCommand();
    void sendAtLine(const QString& line);

    void help(const QString& p);
    void rssi(const QString& p);
    void dongle_ver(const QString& p);
    void dongle_wifi(const QString& p);
    void wifi_rssi(const QString& p);
    void connected(const QString& p);
    void disconnected(const QString& p);
    void WIFI_connected(const QString& p);
    void WIFI_disconnected(const QString& p);
    void SEND_WIFI_DATA(const QString& p);
    void SEND_WIFI_IP(const QString& p);
    void scan_result(const QString& p);

    WriteCallback writeCb_;
    std::map<QString, std::function<void(const QString&)>> commandList_;
    bool isConnected = false;
    bool iswifiConnected = false;
};

#endif // DONGLE_AT_DEVICE_H
