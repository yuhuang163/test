#include "hikvision_scanner_tcp.h"
#include <QElapsedTimer>
#include <QThread>
#include <QCoreApplication>

HikvisionScannerTcp::HikvisionScannerTcp() {}

HikvisionScannerTcp::~HikvisionScannerTcp() {
    disconnectDevice();
}

bool HikvisionScannerTcp::connectDevice(const QString& ip, int port, int timeoutMs, QString* errorMessage) {
    if (isConnected()) {
        disconnectDevice();
    }

    socket_.connectToHost(ip, port);
    if (!socket_.waitForConnected(timeoutMs)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("杩炴帴鎵爜鏋秴鏃? ") + socket_.errorString();
        }
        return false;
    }
    return true;
}

void HikvisionScannerTcp::disconnectDevice() {
    socket_.disconnectFromHost();
    if (socket_.state() != QAbstractSocket::UnconnectedState) {
        socket_.waitForDisconnected(500);
    }
}

bool HikvisionScannerTcp::isConnected() const {
    return socket_.state() == QAbstractSocket::ConnectedState;
}

bool HikvisionScannerTcp::sendStartAndRead(QString* outResult, int timeoutMs, QString* errorMessage) {
    if (!isConnected()) {
        if (errorMessage) *errorMessage = QStringLiteral("鎵爜鏋湭杩炴帴");
        return false;
    }
    
    // 娓呯┖鍙兘鐨勬畫浣欐暟鎹?    socket_.readAll();

    const QByteArray cmd = "start";
    if (socket_.write(cmd) != cmd.size()) {
        if (errorMessage) *errorMessage = QStringLiteral("鍙戦€佹寚浠ゅけ璐? ") + socket_.errorString();
        return false;
    }
    if (!socket_.waitForBytesWritten(timeoutMs)) {
        if (errorMessage) *errorMessage = QStringLiteral("鍙戦€佹寚浠よ秴鏃?);
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    QByteArray buffer;
    while (!timer.hasExpired(timeoutMs)) {
        if (socket_.waitForReadyRead(50)) {
            buffer.append(socket_.readAll());
            QString current = QString::fromLatin1(buffer).trimmed();
            // 鍋囪鏉＄爜澶т簬 10 涓瓧绗﹀氨绠楁壂鍒颁簡锛堟捣搴锋枃妗ｈ鏄?35 瀛楃锛?            if (current.length() > 10) {
                if (outResult) {
                    *outResult = current;
                }
                return true;
            }
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
    }
    
    if (errorMessage) *errorMessage = QStringLiteral("璇诲彇鏉＄爜瓒呮椂锛屽彲鑳芥湭鎵埌");
    return false;
}
