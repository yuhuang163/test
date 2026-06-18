// Qadb.h
#pragma once
#include <QFile>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>
#include "process_channel.h"
#include <functional>

class Qadb : public QObject {
    Q_OBJECT
  public:
    explicit Qadb(QObject* parent = nullptr);
    ~Qadb();

    bool start(); // 启动 adb shell
    void stop();  // 停止 adb shell
    void sendCommand(const QString& cmd,
                     std::function<void(const QString&, qint64)> callback,
                     qint64 timeoutMs = 5000);
    // 新接口：按键监控
    using KeyCallback = std::function<void(const QString& keyName)>;
    // 新接口：通过 adb shell 监控按键
    void startKeyMonitorAdbShell(const QString& deviceEvent, KeyCallback cb);
    void stopKeyMonitorAdbShell();

  private:
    ProcessChannel* channel_ = nullptr;
    struct InputEvent {
        qint64 sec;
        qint64 usec;
        quint16 type;
        quint16 code;
        qint32 value;
    };

    QFile keyDevice;
    QTimer* keyTimer;

    QProcess* keyProcess = nullptr;
    KeyCallback keyCallback;
};
