#ifndef XWD_SUCTION_FIXTURE_DEVICE_H
#define XWD_SUCTION_FIXTURE_DEVICE_H

#include <QSerialPort>
#include <QString>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达 xwd 吸力工站治具：治具串口原文下发（协议待定，步骤配置什么发什么）。 */
class XwdSuctionFixtureDevice {
  public:
    static bool sendRawText(QSerialPort* port, const QString& text, QString* errorMessage = nullptr);
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // XWD_SUCTION_FIXTURE_DEVICE_H
