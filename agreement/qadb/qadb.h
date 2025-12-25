// Qadb.h
#pragma once
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QDateTime>
#include <QTimer>
#include <functional>

struct CmdItem {
    QString command;
    QString endMark;
    QElapsedTimer timer;
    std::function<void(const QString &, qint64)> callback;
    qint64 timeoutMs = 5000; // 默认超时
};

class Qadb : public QObject {
    Q_OBJECT
public:
    explicit Qadb(QObject *parent = nullptr);
    ~Qadb();

    bool start();                 // 启动 adb shell
    void stop();                  // 停止 adb shell
    void sendCommand(const QString &cmd,
                     std::function<void(const QString &, qint64)> callback,
                     qint64 timeoutMs = 5000);
    // 新接口：按键监控
    using KeyCallback = std::function<void(const QString &keyName)>;
    // 新接口：通过 adb shell 监控按键
    void startKeyMonitorAdbShell(const QString &deviceEvent, KeyCallback cb);
    void stopKeyMonitorAdbShell();

private slots:


    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void checkTimeout();

private:
    void processNextCommand();
    QProcess *adbShell = nullptr;
    QQueue<CmdItem> commandQueue;
    bool isProcessing = false;
    QString cmdBuffer;            // 缓存 adb 输出
    QTimer timeoutTimer;
    struct InputEvent {
        qint64 sec;
        qint64 usec;
        quint16 type;
        quint16 code;
        qint32 value;
    };

    QFile keyDevice;
    QTimer *keyTimer;

    QProcess *keyProcess=nullptr;
    KeyCallback keyCallback;
};
