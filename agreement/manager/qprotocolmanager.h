#ifndef QPROTOCOLMANAGER_H
#define QPROTOCOLMANAGER_H

#include "qprotocol.h"
#include "qprotocol_types.h"
#include <QObject>
#include <string>
#include <QByteArray>
#include <QString>

class Qpb;
class Qfctp;
class Qaiot;

class QProtocolManager : public QObject {
    Q_OBJECT
  public:
    explicit QProtocolManager(QObject* parent = nullptr);

    enum class ProtocolType {
        Unknown = 0,
        Qpb,
        Qfctp,
        Qaiot,
    };

    ProtocolType currentProtocolType() const;
    void setCurrentProtocolType(ProtocolType type);

    static ProtocolType protocolTypeFromString(const std::string& value);
    static std::string protocolTypeToString(ProtocolType type);

    void bindQpb(Qpb* pb);
    void bindQfctp(Qfctp* fctp);
    void bindQaiot(Qaiot* aiot);

    qProtocol* currentProtocol() const;
    Qpb* currentQpb() const;
    Qfctp* currentQfctp() const;
    Qaiot* currentQaiot() const;
    bool hasActiveProtocol() const;

    // 统一转发入口：上层只依赖管理器，不直接依赖具体协议实现。
    bool parseCmd(const QByteArray& byte) const;
    bool set(DeviceCmd cmd, const QVariant& data = {}) const;
    bool get(DeviceCmd cmd, const QVariant& param = {}) const;
    bool sendCustomMessage(const QVariantMap& map) const;

    // Qpb 专有能力桥接：用于迁移期兼容旧调用。
    bool setPbMode(int p) const;
    bool setNeedAes(bool needAes) const;
    bool setAppVersion(const QString& version) const;
    QString getAppVersion() const;
    QByteArray aes256Decrypt(const QByteArray& encrypted) const;
    QByteArray aes256Encrypt(const QByteArray& input) const;
    int getState(int stateType, int defaultValue = 0) const;
    bool setState(int stateType, int value) const;
    bool resetAllPb() const;
    bool setShipCount(int count) const;

    bool isQpbProtocolActive() const;
    bool isQfctpProtocolActive() const;
    bool isQaiotProtocolActive() const;

  signals:
    /** 统一上行数据信封（与 qProtocol::reportReceived 对齐） */
    void reportReceived(const ProtocolReport& report);
    /** 传输层 ACK，与结构化 report 分离 */
    void sendGetProductResponse(int data);

  private:
    void syncActivePointer();
    void bindProtocolUpstream(qProtocol* protocol);
    void unbindProtocolUpstream(qProtocol* protocol);

    ProtocolType currentType_ = ProtocolType::Qpb;
    Qpb* qpb_ = nullptr;
    Qfctp* qfctp_ = nullptr;
    Qaiot* qaiot_ = nullptr;
    qProtocol* active_ = nullptr;
};

#endif // QPROTOCOLMANAGER_H
