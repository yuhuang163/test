#ifndef QATMANAGER_H
#define QATMANAGER_H

#include <QObject>
#include <QByteArray>
#include "../device/dongle/dongle_at_device.h"
#include "../codec/at_line_codec.h"
#include "../codec/at_suction_frame_codec.h"

class QSerialPort;

class QatManager : public QObject {
    Q_OBJECT
  public:
    explicit QatManager(QSerialPort* port = nullptr, QObject* parent = nullptr);

    void parseCmd(const QByteArray& byte);
    // clang-format off
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
    // clang-format on
  signals:
    void reportReceived(const ProtocolReport& report);
    void sendGetProductResponse(int data);

  private:
    DongleAtDevice device_;
    AtLineCodec codec_;
    AtSuctionFrameCodec suctionCodec_;
};

#endif // QATMANAGER_H
