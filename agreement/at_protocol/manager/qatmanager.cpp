#include "qatmanager.h"

QatManager::QatManager(QObject* parent) : QObject(parent) {
    connect(&device_, &DongleAtDevice::reportReceived, this, &QatManager::reportReceived);
    connect(&device_, &DongleAtDevice::sendGetProductResponse, this, &QatManager::sendGetProductResponse);
}

void QatManager::setWriteCallback(const DongleAtDevice::WriteCallback& cb) {
    device_.setWriteCallback(cb);
}

void QatManager::parseCmd(const QByteArray& byte) {
    codec_.feed(byte, [this](const AtFrame& frame) {
        device_.onFrameReceived(frame);
    });
}
