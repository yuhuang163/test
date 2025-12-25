// Qshell.h
#pragma once
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QElapsedTimer>
#include <QTimer>
#include <functional>

class Qshell : public QObject
{
    Q_OBJECT
public:
    explicit Qshell(QObject *parent = nullptr);
    ~Qshell();

    bool start();   // 启动 cmd / powershell
    void stop();

    void sendCommand(const QString &cmd,
                     std::function<void(const QString &, qint64)> callback,
                     qint64 timeoutMs = 3000);

private slots:
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void checkTimeout();

private:
    struct CmdItem {
        QString command;
        QString endMark;
        QElapsedTimer timer;
        std::function<void(const QString &, qint64)> callback;
        qint64 timeoutMs;
    };

    void processNextCommand();

private:
    QProcess *shell = nullptr;
    QQueue<CmdItem> commandQueue;
    QString cmdBuffer;
    bool isProcessing = false;
    QTimer timeoutTimer;
};
