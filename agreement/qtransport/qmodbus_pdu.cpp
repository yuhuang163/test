#include "qmodbus_pdu.h"

namespace QModbusPdu {

void appendUint16Be(QByteArray& b, quint16 v) {
    b.append(char(quint8(v >> 8)));
    b.append(char(quint8(v & 0xFF)));
}

quint16 readUint16Be(const char* p) {
    return (quint8(p[0]) << 8) | quint8(p[1]);
}

QByteArray buildReadCoilsRequestPdu(quint16 startAddress, quint16 quantity) {
    QByteArray pdu;
    pdu.append(char(kFcReadCoils));
    appendUint16Be(pdu, startAddress);
    appendUint16Be(pdu, quantity);
    return pdu;
}

QByteArray buildWriteSingleCoilRequestPdu(quint16 address, bool value) {
    QByteArray pdu;
    pdu.append(char(kFcWriteSingleCoil));
    appendUint16Be(pdu, address);
    appendUint16Be(pdu, value ? quint16(0xFF00) : quint16(0x0000));
    return pdu;
}

quint16 crc16ModbusRtuBigEndian(const QByteArray& data) {
    quint16 crc = 0xFFFF;
    for (char c : data) {
        crc ^= static_cast<quint8>(c);
        for (int j = 0; j < 8; ++j) {
            if (crc & 0x0001) {
                crc = (crc >> 1) ^ 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return ((crc & 0x00FF) << 8) | ((crc & 0xFF00) >> 8);
}

bool isExceptionResponse(const QByteArray& pdu, quint8* exceptionCode) {
    if (pdu.isEmpty()) {
        return false;
    }
    const quint8 fc = quint8(pdu[0]);
    const bool isException = (fc & 0x80) != 0;
    if (isException && exceptionCode) {
        *exceptionCode = pdu.size() > 1 ? quint8(pdu[1]) : 0;
    }
    return isException;
}

bool functionCodeMatches(const QByteArray& pdu, quint8 expectedFunctionCode) {
    return !pdu.isEmpty() && quint8(pdu[0]) == expectedFunctionCode;
}

QString exceptionCodeText(quint8 exceptionCode) {
    switch (exceptionCode) {
    case 0x01:
        return QStringLiteral("非法功能码");
    case 0x02:
        return QStringLiteral("非法数据地址");
    case 0x03:
        return QStringLiteral("非法数据值");
    case 0x04:
        return QStringLiteral("从站设备故障");
    case 0x05:
        return QStringLiteral("确认");
    case 0x06:
        return QStringLiteral("从站忙");
    case 0x08:
        return QStringLiteral("存储奇偶性错误");
    case 0x0A:
        return QStringLiteral("不可用网关路径");
    case 0x0B:
        return QStringLiteral("网关目标设备响应失败");
    default:
        return QStringLiteral("未知异常码");
    }
}

QString formatExceptionMessage(const QByteArray& pdu) {
    quint8 ex = 0;
    if (!isExceptionResponse(pdu, &ex)) {
        return QString();
    }
    const quint8 fc = quint8(pdu[0]);
    return QStringLiteral("功能码=0x%1 异常码=%2(%3)")
        .arg(fc, 2, 16, QChar('0'))
        .arg(ex)
        .arg(exceptionCodeText(ex));
}

bool parseReadCoilsPdu(const QByteArray& pdu, int quantity, QVector<bool>* out, QString* errorMessage) {
    if (!out || quantity <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈参数非法");
        }
        return false;
    }
    out->clear();
    if (pdu.size() < 2) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈响应过短");
        }
        return false;
    }
    if (isExceptionResponse(pdu)) {
        if (errorMessage) {
            *errorMessage = formatExceptionMessage(pdu);
        }
        return false;
    }
    if (!functionCodeMatches(pdu, kFcReadCoils)) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈功能码错误");
        }
        return false;
    }
    const int byteCount = quint8(pdu[1]);
    const int expectByteCount = (quantity + 7) / 8;
    if (byteCount < expectByteCount) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈字节数不足");
        }
        return false;
    }
    if (pdu.size() < 2 + byteCount) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("读线圈数据长度不足");
        }
        return false;
    }
    for (int i = 0; i < quantity; ++i) {
        const int byteIndex = i / 8;
        const int bitIndex = i % 8;
        const bool bit = (quint8(pdu[2 + byteIndex]) & (1 << bitIndex)) != 0;
        out->append(bit);
    }
    return true;
}

bool validateRtuFrame(const QByteArray& frame, quint8* outByteCount) {
    constexpr int kModbusMinFrameLen = 5;
    if (frame.size() < kModbusMinFrameLen) {
        return false;
    }
    const quint8 byteCount = quint8(frame[2]);
    if (frame.size() != kModbusMinFrameLen + byteCount) {
        return false;
    }
    const quint16 crcExpected = readUint16Be(frame.constData() + frame.size() - 2);
    const quint16 crcCalculated = crc16ModbusRtuBigEndian(frame.left(frame.size() - 2));
    if (crcCalculated != crcExpected) {
        return false;
    }
    if (outByteCount) {
        *outByteCount = byteCount;
    }
    return true;
}

}  // namespace QModbusPdu
