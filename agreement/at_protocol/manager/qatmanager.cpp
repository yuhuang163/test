#include "qatmanager.h"

QatManager::QatManager(QSerialPort* port, QObject* parent) : QObject(parent), device_(port, this) {
    connect(&device_, &DongleAtDevice::reportReceived, this, &QatManager::reportReceived);
    connect(&device_, &DongleAtDevice::sendGetProductResponse, this, &QatManager::sendGetProductResponse);
}

void QatManager::parseCmd(const QByteArray& byte) {
    suctionCodec_.feed(byte, [this](const ProtocolDongleSuctionData& data) {
        emit reportReceived(ProtocolReport(QStringLiteral("ProtocolDongleSuctionData"), QVariant::fromValue(data)));
    });
    codec_.feed(byte, [this](const AtFrame& frame) {
        device_.onFrameReceived(frame);
    });
}
