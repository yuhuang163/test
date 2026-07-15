#include "xwd_ble_fixture_device.h"

#include "qdebug.h"
#include "xwd_ble_uart_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

bool XwdBleFixtureDevice::sendRawText(QSerialPort* port, const QString& text, QString* errorMessage) {
    if (!port) {
        if (errorMessage)
            *errorMessage = QStringLiteral("治具串口未初始化");
        return false;
    }
    if (!port->isOpen()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("治具串口未打开");
        return false;
    }

    const QByteArray frame = XwdBleUartCodec::encodeRawText(text);
    if (frame.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("发送内容为空");
        return false;
    }

    qDebug().noquote() << "XWD_BLE FIXTURE TX:" << QString::fromLatin1(frame.toHex(' ').toUpper());
    qDebug().noquote() << "XWD_BLE FIXTURE TX(text):" << text;
    const qint64 written = port->write(frame);
    if (written != frame.size()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("治具串口写入失败");
        return false;
    }
    if (!port->waitForBytesWritten(2000)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("治具串口写入超时");
        return false;
    }
    return true;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
