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
            *errorMessage = QStringLiteral("连接扫码枪超时: ") + socket_.errorString();
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
        if (errorMessage) *errorMessage = QStringLiteral("扫码枪未连接");
        return false;
    }
    
    // 清空可能的残余数据
    socket_.readAll();

    const QByteArray cmd = "start";
    if (socket_.write(cmd) != cmd.size()) {
        if (errorMessage) *errorMessage = QStringLiteral("发送指令失败: ") + socket_.errorString();
        return false;
    }
    if (!socket_.waitForBytesWritten(timeoutMs)) {
        if (errorMessage) *errorMessage = QStringLiteral("发送指令超时");
        return false;
    }

    QElapsedTimer timer;
    timer.start();
    QByteArray buffer;
    while (!timer.hasExpired(timeoutMs)) {
        if (socket_.waitForReadyRead(50)) {
            buffer.append(socket_.readAll());
            QString current = QString::fromLatin1(buffer).trimmed();
            // 假设条码大于 10 个字符就算扫到了（海康文档说明 35 字符）
            if (current.length() > 10) {
                if (outResult) {
                    *outResult = current;
                }
                return true;
            }
        }
        QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
    }
    
    if (errorMessage) *errorMessage = QStringLiteral("读取条码超时，可能未扫到");
    return false;
}
