// Qshell.h
#pragma once
#include <QObject>
#include <QString>
#include <functional>
#include "qprocesschannel.h"

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

private:
    QProcessChannel* channel_ = nullptr;
};
