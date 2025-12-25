// Qadb.cpp
#include "Qadb.h"
#include <QDebug>
#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif
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

    // 获取程序运行目录
    QString exeDir = QCoreApplication::applicationDirPath();

    // 拼接 adb 路径
    QString adbPath = exeDir + R"(\产线调试包v4\adb\adb.exe)";

    adbShell->start(adbPath, {"shell"});

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

void Qadb::onFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "adb shell finished:" << exitCode << exitStatus;

    while (!commandQueue.isEmpty()) {
        // ❶ 先拷贝出来（或 move）
        CmdItem item = commandQueue.dequeue();

        // ❷ 再调用 callback（此时队列已安全）
        if (item.callback) {
            item.callback("ADB shell被中断", item.timer.elapsed());
        }
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
        // 3️⃣ 杀掉当前 adb shell（关键！）
        if (adbShell->state() != QProcess::NotRunning) {
            adbShell->kill();
            adbShell->waitForFinished(1000);
        }

        // 4️⃣ 重新启动 adb shell
        start();
        processNextCommand();
    }
}
void Qadb::startKeyMonitorAdbShell(const QString &deviceEvent, KeyCallback cb)
{
    if(keyProcess) {
        // qDebug() << "已经在监控";
        return;} // 已经在监控

    keyCallback = cb;

    keyProcess = new QProcess(this);

    // adb shell 命令，每次输出一条事件
    QString cmd = QString("adb shell cat %1").arg(deviceEvent);

    keyProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(keyProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray data = keyProcess->readAllStandardOutput();

        // 解析按键数据 (16/24字节结构根据 event size)
        // 这里简单演示，只判断 Power/ Shutter key
        // Power key = code 116, Shutter key = code 355
        for(int i=0; i+24 <= data.size(); i+=24) {
            const char *p = data.constData() + i;
            quint16 type = *(quint16*)(p+16);
            quint16 code = *(quint16*)(p+18);
            quint32 value = *(quint32*)(p+20);

            if(type != 1) continue; // EV_KEY
            if(code == 357 && value == 1) {
                if(keyCallback) keyCallback("power_key");
            }
            if(code == 355 && value == 1) {
                if(keyCallback) keyCallback("shutter_key");
            }
        }
    });
    connect(keyProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
qDebug() << "adb shell finished:";
                // USB拔插导致cat结束时，清理并延时重启
                keyProcess->kill();
                keyProcess->deleteLater();
                keyProcess = nullptr;
            });

    keyProcess->start(cmd);
}

void Qadb::stopKeyMonitorAdbShell()
{
    if(keyProcess) {
        keyProcess->kill();
        keyProcess->deleteLater();
        keyProcess = nullptr;
    }
}
