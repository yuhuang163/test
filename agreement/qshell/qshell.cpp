// Qshell.cpp
#include "Qshell.h"
#include <QDateTime>
#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

Qshell::Qshell(QObject *parent) : QObject(parent)
{
    shell = new QProcess(this);

    // 合并 stdout / stderr
    shell->setProcessChannelMode(QProcess::MergedChannels);

    connect(shell, &QProcess::readyReadStandardOutput,
            this, &Qshell::onReadyRead);

    connect(shell,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Qshell::onFinished);

    connect(&timeoutTimer, &QTimer::timeout,
            this, &Qshell::checkTimeout);

    timeoutTimer.start(500);
}

Qshell::~Qshell()
{
    stop();
}

bool Qshell::start()
{
    if (shell->state() != QProcess::NotRunning)
        return true;

    QString program = "powershell.exe";
    QStringList args = {
        "-NoLogo",
        "-NoProfile",
        "-ExecutionPolicy", "Bypass",
        "-NoExit",          // ⭐ 必须
        "-Command", "-"     // ⭐ stdin
    };

    shell->start(program, args);

    if (!shell->waitForStarted(3000)) {
        qDebug() << "Failed to start PowerShell";
        return false;
    }

    // ===== 初始化环境（只做一次）=====
    shell->write(
        "$ProgressPreference='SilentlyContinue'\n"
        "$ErrorActionPreference='Continue'\n"
        "[Console]::OutputEncoding=[Text.UTF8Encoding]::new()\n"
        );

    qDebug() << "Persistent PowerShell started";
    return true;
}


void Qshell::stop()
{
    if (shell && shell->state() != QProcess::NotRunning) {
        shell->write("exit\n");
        shell->waitForFinished(1000);
    }

    commandQueue.clear();
    isProcessing = false;
    cmdBuffer.clear();
}

void Qshell::sendCommand(const QString &cmd,
                         std::function<void(const QString &, qint64)> callback,
                         qint64 timeoutMs)
{
     // qDebug() << "sendCommand:" << cmd ;
    CmdItem item;
    item.command = cmd;
    item.endMark = QString("__CMD_END_%1__")
                       .arg(QDateTime::currentMSecsSinceEpoch());
    item.timer.start();
    item.callback = callback;
    item.timeoutMs = timeoutMs;

    commandQueue.enqueue(item);

    if (!isProcessing)
        processNextCommand();
}

void Qshell::processNextCommand()
{
    if (commandQueue.isEmpty()) {
        isProcessing = false;
        return;
    }

    isProcessing = true;

    CmdItem &item = commandQueue.head();
    item.timer.restart();

    // ⚠️ 不要用 echo（有 alias 风险）
    QString fullCmd =
        item.command +
        " ; echo " + item.endMark;

    // qDebug() << "[Qshell] exec:" << fullCmd;

    shell->write((fullCmd + "\n").toUtf8());
}

void Qshell::onReadyRead()
{
    QByteArray output = shell->readAllStandardOutput();
    if (output.isEmpty())
        return;

    cmdBuffer += QString::fromUtf8(output);

    if (commandQueue.isEmpty())
        return;

    // 拷贝 CmdItem 避免引用悬空
    CmdItem item = commandQueue.head();
    int idx = cmdBuffer.indexOf(item.endMark);
    if (idx != -1) {

        QString result = cmdBuffer.left(idx).trimmed();

        // 先清理缓冲和队列
        cmdBuffer.remove(0, idx + item.endMark.length());
        commandQueue.dequeue();

        // 触发下一个命令
        processNextCommand();

        // 最后执行回调
        if (item.callback)
            item.callback(result, item.timer.elapsed());
    }
}

void Qshell::onFinished(int, QProcess::ExitStatus)
{
    qDebug() << "[Qshell] PowerShell finished";

    while (!commandQueue.isEmpty()) {
        CmdItem item = commandQueue.dequeue();
        if (item.callback)
            item.callback("PowerShell exited", item.timer.elapsed());
    }

    isProcessing = false;
    cmdBuffer.clear();
}

void Qshell::checkTimeout()
{
    if (commandQueue.isEmpty())
        return;

    CmdItem &item = commandQueue.head();
    if (item.timer.elapsed() > item.timeoutMs) {

        if (item.callback)
            item.callback("命令超时", item.timer.elapsed());

        commandQueue.dequeue();
        isProcessing = false;
        cmdBuffer.clear();

        processNextCommand();
    }
}
