#ifndef QMODBUS_PDU_H
#define QMODBUS_PDU_H

#include <QByteArray>
#include <QString>
#include <QVector>

namespace QModbusPdu {

constexpr quint8 kFcReadCoils = 0x01;
constexpr quint8 kFcWriteSingleCoil = 0x05;

void appendUint16Be(QByteArray& b, quint16 v);
quint16 readUint16Be(const char* p);

QByteArray buildReadCoilsRequestPdu(quint16 startAddress, quint16 quantity);
QByteArray buildWriteSingleCoilRequestPdu(quint16 address, bool value);

// Modbus RTU CRC16（返回值与帧尾两个字节按高位在前比较一致）
quint16 crc16ModbusRtuBigEndian(const QByteArray& data);

// 常见响应检查工具
bool isExceptionResponse(const QByteArray& pdu, quint8* exceptionCode = nullptr);
bool functionCodeMatches(const QByteArray& pdu, quint8 expectedFunctionCode);
QString exceptionCodeText(quint8 exceptionCode);
QString formatExceptionMessage(const QByteArray& pdu);

// RTU 基础校验：最小长度/字节数/CRC
bool validateRtuFrame(const QByteArray& frame, quint8* outByteCount = nullptr);

// 读线圈响应 PDU（仅功能码+字节数+数据，不含 TCP/RTU 头尾）
bool parseReadCoilsPdu(const QByteArray& pdu, int quantity, QVector<bool>* out, QString* errorMessage = nullptr);

} // namespace QModbusPdu

#endif // QMODBUS_PDU_H
