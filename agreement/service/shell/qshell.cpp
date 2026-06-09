// Qshell.cpp
#include "Qshell.h"
#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

Qshell::Qshell(QObject *parent) : QObject(parent)
{
    channel_ = new QProcessChannel(this);
    channel_->setTxPrefix(QStringLiteral("SHELL TX:"));
    channel_->setRxPrefix(QStringLiteral("SHELL RX:"));
    const QByteArray initCommand =
        "$ProgressPreference='SilentlyContinue'\n"
        "$ErrorActionPreference='Continue'\n"
        "[Console]::OutputEncoding=[Text.UTF8Encoding]::new()\n";
    channel_->setInitCommand(initCommand);
}

Qshell::~Qshell()
{
    stop();
}

bool Qshell::start()
{
    QString program = "powershell.exe";
    QStringList args = {
        "-NoLogo",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-NoExit",          // ⭐ 必须
        "-Command", "-"     // ⭐ stdin
    };
    const bool ok = channel_->start(program, args, 3000);
    if (ok) {
        qDebug() << "Persistent PowerShell started";
    } else {
        qDebug() << "Failed to start PowerShell";
    }
    return ok;
}


void Qshell::stop()
{
    if (channel_) {
        channel_->stop();
    }
}

void Qshell::sendCommand(const QString &cmd,
                         std::function<void(const QString &, qint64)> callback,
                         qint64 timeoutMs)
{
    if (channel_) {
        channel_->sendCommand(cmd, std::move(callback), timeoutMs);
    }
}
