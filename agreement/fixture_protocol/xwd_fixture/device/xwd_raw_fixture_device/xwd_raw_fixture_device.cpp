#include "xwd_raw_fixture_device.h"

#include "qdebug.h"
#include "xwd_raw_uart_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

bool XwdRawFixtureDevice::sendRawText(QSerialPort* port, const QString& text, QString* errorMessage) {
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

    bool parsedAsHex = false;
    const QByteArray frame = XwdRawUartCodec::encodeRawText(text, &parsedAsHex);
    if (frame.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("发送内容为空");
        return false;
    }

    // 原文：只打配置字符串；十六进制：打解出来的字节
    if (parsedAsHex)
        qDebug().noquote() << "XWD FIXTURE TX(十六进制):" << QString::fromLatin1(frame.toHex(' ').toUpper());
    else
        qDebug().noquote() << "XWD FIXTURE TX(原文):" << text.trimmed();

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
