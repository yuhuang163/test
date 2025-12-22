// Qadb.cpp
#include "Qadb.h"
#include <QDebug>

Qadb::Qadb(QObject *parent) : QObject(parent) {
    adbShell = new QProcess(this);
    adbShell->setProcessChannelMode(QProcess::MergedChannels);
    connect(adbShell, &QProcess::readyReadStandardOutput, this, &Qadb::onReadyRead);
    connect(adbShell, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Qadb::onFinished);

    connect(&timeoutTimer, &QTimer::timeout, this, &Qadb::checkTimeout);
    timeoutTimer.start(500); // 每 500ms 检查超时
}

Qadb::~Qadb() {
    stop();
}

bool Qadb::start() {
    if(adbShell->state() != QProcess::NotRunning) return true;

    adbShell->start("adb", {"shell"});
    if(!adbShell->waitForStarted(3000)) {
        qDebug() << "Failed to start adb shell!";
        return false;
    }
    qDebug() << "adb shell started";
    return true;
}

void Qadb::stop() {
    if(adbShell && adbShell->state() != QProcess::NotRunning) {
        adbShell->write("exit\n");
        adbShell->waitForFinished(1000);
    }
    commandQueue.clear();
    isProcessing = false;
    cmdBuffer.clear();
}



void Qadb::sendCommand(const QString &cmd,
                       std::function<void(const QString &, qint64)> callback,
                       qint64 timeoutMs) {

    QString endMark = QString("__CMD_END_%1__").arg(QDateTime::currentMSecsSinceEpoch());
    CmdItem item;
    item.command = cmd;
    item.endMark = endMark;
    item.timer.start();
    item.callback = callback;
    item.timeoutMs = timeoutMs;

    commandQueue.enqueue(item);

    if(!isProcessing) processNextCommand();
}

void Qadb::processNextCommand() {
    if(commandQueue.isEmpty()) {
        isProcessing = false;
        return;
    }

    isProcessing = true;
    CmdItem &item = commandQueue.head();
    QString fullCmd = item.command + " ; echo " + item.endMark;
    adbShell->write((fullCmd + "\n").toUtf8());
}

void Qadb::onReadyRead() {
    QByteArray output = adbShell->readAllStandardOutput();
    if(output.isEmpty()) return;

    cmdBuffer += QString(output);

    if(!commandQueue.isEmpty()) {
        CmdItem &item = commandQueue.head();
        if(cmdBuffer.contains(item.endMark)) {
            QString result = cmdBuffer;
            result.replace(item.endMark, "").trimmed();
            if(item.callback) item.callback(result, item.timer.elapsed());
            commandQueue.dequeue();
            cmdBuffer.clear();
            isProcessing = false;
            processNextCommand();
        }
    }
}

void Qadb::onFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    qDebug() << "adb shell finished:" << exitCode << exitStatus;
    // shell 被终止，回调所有队列
    while(!commandQueue.isEmpty()) {
        CmdItem &item = commandQueue.head();
        if(item.callback) item.callback("ADB shell被中断", item.timer.elapsed());
        commandQueue.dequeue();
    }
    isProcessing = false;
    cmdBuffer.clear();
}

void Qadb::checkTimeout() {
    if(commandQueue.isEmpty()) return;
    CmdItem &item = commandQueue.head();
    if(item.timer.elapsed() > item.timeoutMs) {
        if(item.callback) item.callback("命令超时", item.timer.elapsed());
        commandQueue.dequeue();
        isProcessing = false;
        cmdBuffer.clear();
        processNextCommand();
    }
}
