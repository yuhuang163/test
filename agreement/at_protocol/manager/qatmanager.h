#ifndef QATMANAGER_H
#define QATMANAGER_H

#include <QObject>
#include <QByteArray>
#include "dongle_at_device.h"
#include "dongle_at_codec.h"

class QatManager : public QObject {
    Q_OBJECT
public:
    explicit QatManager(QObject* parent = nullptr);

    /** 提供底层写入接口给 Device */
    void setWriteCallback(const DongleAtDevice::WriteCallback& cb);

    /** 解析底层收到的字节流 */
    void parseCmd(const QByteArray& byte);

    DongleAtDevice* device() { return &device_; }

    void set(DongleCmd cmd, const QVariant& data = {}) { device_.set(cmd, data); }
    void get(DongleCmd cmd, const QVariant& param = {}) { device_.get(cmd, param); }
    bool sendCustomMessage(const QVariantMap& map) { return device_.sendCustomMessage(map); }

    bool getConnected() const { return device_.getConnected(); }
    void resetConnected() { device_.resetConnected(); }
    void setConnected() { device_.setConnected(); }

    bool getwifiConnected() const { return device_.getwifiConnected(); }
    void resetwifiConnected() { device_.resetwifiConnected(); }
    void setwifiConnected() { device_.setwifiConnected(); }

signals:
    void reportReceived(const ProtocolReport& report);
    void sendGetProductResponse(int data);

private:
    DongleAtDevice device_;
    DongleAtCodec codec_;
};

#endif // QATMANAGER_H
