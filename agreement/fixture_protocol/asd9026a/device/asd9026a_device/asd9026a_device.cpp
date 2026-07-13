#include "asd9026a_device.h"

#include "Abini.h"
#include "asd9026a_codec.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QtGlobal>
#include <cmath>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

double scaleByUnit(quint32 raw, quint8 unitCode) {
    switch (unitCode) {
    case 1:
        return raw;
    case 2:
        return raw / 1000.0;
    case 3:
        return raw / 1000000.0;
    case 4:
        return raw / 1000000000.0;
    default:
        return raw;
    }
}

} // namespace

Asd9026aDevice::Asd9026aDevice(QObject* parent) : QObject(parent) {
    requestTimeoutMs_ = SETTINGS.value(QStringLiteral("ASD9026A/RequestTimeoutMs"), 2000).toInt();
}

Asd9026aDevice::~Asd9026aDevice() {
    close();
}

bool Asd9026aDevice::open(const QString& portName, int baudRate, QString* errorMessage) {
    close();
    if (portName.trimmed().isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 串口未配置");
        return false;
    }

    serial_.setPortName(portName.trimmed());
    serial_.setBaudRate(baudRate);
    serial_.setDataBits(QSerialPort::Data8);
    serial_.setParity(QSerialPort::NoParity);
    serial_.setStopBits(QSerialPort::OneStop);
    serial_.setFlowControl(QSerialPort::NoFlowControl);

    if (!serial_.open(QIODevice::ReadWrite)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 打开串口失败: %1").arg(serial_.errorString());
        return false;
    }
    return true;
}

void Asd9026aDevice::close() {
    if (serial_.isOpen())
        serial_.close();
}

bool Asd9026aDevice::isOpen() const {
    return serial_.isOpen();
}

QString Asd9026aDevice::portName() const {
    return serial_.portName();
}

int Asd9026aDevice::baudRate() const {
    return serial_.baudRate();
}

bool Asd9026aDevice::transact(const QByteArray& request, QByteArray* response, QString* errorMessage) {
    if (!serial_.isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 串口未打开");
        return false;
    }

    serial_.clear();
    qDebug().noquote() << "ASD9026A TX:" << request.toHex(' ');
    if (serial_.write(request) != request.size()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 发送失败: %1").arg(serial_.errorString());
        return false;
    }
    if (!serial_.waitForBytesWritten(requestTimeoutMs_)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 发送超时");
        return false;
    }

    QByteArray buffer;
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < requestTimeoutMs_) {
        if (serial_.waitForReadyRead(qMax(20, requestTimeoutMs_ - static_cast<int>(timer.elapsed()))))
            buffer.append(serial_.readAll());
        if (buffer.size() >= 6) {
            const int payloadLen = static_cast<quint8>(buffer.at(3));
            const int frameLen = 4 + payloadLen + 2;
            if (buffer.size() >= frameLen) {
                const QByteArray frame = buffer.left(frameLen);
                qDebug().noquote() << "ASD9026A RX:" << frame.toHex(' ');
                if (response)
                    *response = frame;
                return true;
            }
        }
    }

    if (errorMessage) {
        *errorMessage = buffer.isEmpty() ? QStringLiteral("ASD9026A 接收超时")
                                         : QStringLiteral("ASD9026A 接收不完整帧");
    }
    return false;
}

bool Asd9026aDevice::setOutputEnabled(quint8 moduleAddr, bool enabled, QString* errorMessage) {
    QByteArray payload;
    payload.append(enabled ? char(0x01) : char(0x00));
    payload.append(char(0x00));
    payload.append(char(0x00));

    const QByteArray request =
        Asd9026aCodec::buildFrame(moduleAddr, Asd9026aCodec::kFuncWrite, Asd9026aCodec::kCmdOutputSwitch, payload);
    QByteArray response;
    if (!transact(request, &response, errorMessage))
        return false;

    quint8 addr = 0;
    quint8 func = 0;
    quint8 cmd = 0;
    QByteArray body;
    if (!Asd9026aCodec::parseFrame(response, &addr, &func, &cmd, &body, errorMessage))
        return false;
    if (addr != moduleAddr) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 应答地址不匹配");
        return false;
    }
    return true;
}

bool Asd9026aDevice::configureConstantVoltage(quint8 moduleAddr, double voltageVolts, double currentLimitAmps,
                                              QString* errorMessage) {
    const quint32 voltageUv = static_cast<quint32>(qMax(0.0, voltageVolts) * 1000000.0);
    const quint32 currentMa = static_cast<quint32>(qMax(0.0, currentLimitAmps) * 1000.0);

    QByteArray payload;
    payload.append(char(0x00)); // 恒压
    payload.append(Asd9026aCodec::appendLe32(voltageUv));
    payload.append(Asd9026aCodec::appendLe32(currentMa));
    payload.append(char(0x04)); // 电流档位：自动
    payload.append(char(0x01)); // 显示速度：快
    payload.append(char(0x00));
    payload.append(char(0x00)); // 线阻 0

    const QByteArray request =
        Asd9026aCodec::buildFrame(moduleAddr, Asd9026aCodec::kFuncAnalogWrite, Asd9026aCodec::kCmdAnalogConfigure, payload);
    QByteArray response;
    if (!transact(request, &response, errorMessage))
        return false;

    quint8 addr = 0;
    quint8 func = 0;
    quint8 cmd = 0;
    QByteArray body;
    if (!Asd9026aCodec::parseFrame(response, &addr, &func, &cmd, &body, errorMessage))
        return false;
    if (addr != moduleAddr || func != Asd9026aCodec::kFuncAnalogReply) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 配置应答功能码异常");
        return false;
    }
    return true;
}

bool Asd9026aDevice::readModuleStatus(quint8 moduleAddr, int* outputMilliVolts, int* outputMilliAmps,
                                      QString* errorMessage) {
    const QByteArray request =
        Asd9026aCodec::buildFrame(moduleAddr, Asd9026aCodec::kFuncRead, Asd9026aCodec::kCmdQueryModuleStatus);
    QByteArray response;
    if (!transact(request, &response, errorMessage))
        return false;

    quint8 addr = 0;
    quint8 func = 0;
    quint8 cmd = 0;
    QByteArray body;
    if (!Asd9026aCodec::parseFrame(response, &addr, &func, &cmd, &body, errorMessage))
        return false;
    if (addr != moduleAddr || func != Asd9026aCodec::kFuncReadReply || cmd != Asd9026aCodec::kCmdQueryModuleStatus) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 状态应答不匹配");
        return false;
    }
    if (body.size() < 3) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 状态数据长度不足");
        return false;
    }

    if (outputMilliVolts)
        *outputMilliVolts = static_cast<quint8>(body.at(1)) | (static_cast<quint8>(body.at(2)) << 8);
    if (outputMilliAmps)
        *outputMilliAmps = 0;
    return true;
}

bool Asd9026aDevice::readAnalogStatus(quint8 moduleAddr, Asd9026aAnalogStatus* out, QString* errorMessage) {
    const QByteArray request =
        Asd9026aCodec::buildFrame(moduleAddr, Asd9026aCodec::kFuncAnalogRead, Asd9026aCodec::kCmdAnalogStatus);
    QByteArray response;
    if (!transact(request, &response, errorMessage))
        return false;

    quint8 addr = 0;
    quint8 func = 0;
    quint8 cmd = 0;
    QByteArray body;
    if (!Asd9026aCodec::parseFrame(response, &addr, &func, &cmd, &body, errorMessage))
        return false;
    if (addr != moduleAddr || func != Asd9026aCodec::kFuncAnalogReply || cmd != Asd9026aCodec::kCmdAnalogStatus) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 模拟电池状态应答不匹配");
        return false;
    }
    if (body.size() < 11) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ASD9026A 模拟电池状态长度不足");
        return false;
    }

    if (out) {
        out->outputOn = static_cast<quint8>(body.at(0)) != 0;
        const quint32 voltageRaw = Asd9026aCodec::readLe32(body, 1);
        const quint8 voltageUnit = static_cast<quint8>(body.at(5));
        const quint32 currentRaw = Asd9026aCodec::readLe32(body, 6);
        const quint8 currentUnit = static_cast<quint8>(body.at(10));
        out->voltage = scaleByUnit(voltageRaw, voltageUnit);
        out->current = scaleByUnit(currentRaw, currentUnit);
        if (body.size() > 12) {
            const int tempRaw = static_cast<quint8>(body.at(12));
            out->temperatureC = tempRaw == 0xFF ? 0 : tempRaw - 40;
        }
    }
    return true;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
