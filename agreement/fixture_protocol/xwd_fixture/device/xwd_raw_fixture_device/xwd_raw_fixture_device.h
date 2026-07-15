#ifndef XWD_RAW_FIXTURE_DEVICE_H
#define XWD_RAW_FIXTURE_DEVICE_H

#include <QSerialPort>
#include <QString>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达 XWD 治具：治具串口原文/十六进制下发（蓝牙盒与吸力等共用同一物理层）。 */
class XwdRawFixtureDevice {
  public:
    static bool sendRawText(QSerialPort* port, const QString& text, QString* errorMessage = nullptr);
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // XWD_RAW_FIXTURE_DEVICE_H
