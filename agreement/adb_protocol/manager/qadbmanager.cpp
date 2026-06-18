// Qadb.cpp
#include "qadbmanager.h"
#include <QCoreApplication>
#include <QDebug>
#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif
Qadb::Qadb(QObject* parent) : QObject(parent) {
    channel_ = new ProcessChannel(this);
    channel_->setTxPrefix(QStringLiteral("ADB TX:"));
    channel_->setRxPrefix(QStringLiteral("ADB RX:"));
    // 保持旧行为：命令超时后重启 adb shell，再继续后续队列命令。
    channel_->setRestartOnTimeout(true, 3000);
}

Qadb::~Qadb() {
    stop();
}

bool Qadb::start() {
    // 获取程序运行目录
    QString exeDir = QCoreApplication::applicationDirPath();

    // 拼接 adb 路径
    QString adbPath = exeDir + R"(\factorydebugv4\adb\adb.exe)";
    if (!channel_->start(adbPath, {"shell"}, 3000)) {
        qDebug() << "Failed to start adb shell!";
        return false;
    }
    // qDebug() << "adb shell started";
    return true;
}

void Qadb::stop() {
    if (channel_) {
        channel_->stop();
    }
}

void Qadb::sendCommand(const QString& cmd,
                       std::function<void(const QString&, qint64)> callback,
                       qint64 timeoutMs) {
    if (channel_) {
        channel_->sendCommand(cmd, callback, timeoutMs);
    }
}
void Qadb::startKeyMonitorAdbShell(const QString& deviceEvent, KeyCallback cb) {
    if (keyProcess) {
        // qDebug() << "已经在监控";
        return;
    } // 已经在监控

    keyCallback = cb;

    keyProcess = new QProcess(this);

    // adb shell 命令，每次输出一条事件
    QString cmd = deviceEvent;

    keyProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(keyProcess, &QProcess::readyReadStandardOutput, this, [this]() {
        QByteArray data = keyProcess->readAllStandardOutput();
        if (!data.isEmpty()) {
            qDebug().noquote() << "ADB RX:" << QString::fromLatin1(data.toHex(' ').toUpper());
        }

        // 解析按键数据 (16/24字节结构根据 event size)
        // 这里简单演示，只判断 Power/ Shutter key
        // Power key = code 116, Shutter key = code 355
        for (int i = 0; i + 24 <= data.size(); i += 24) {
            const char* p = data.constData() + i;
            quint16 type = *(quint16*)(p + 16);
            quint16 code = *(quint16*)(p + 18);
            quint32 value = *(quint32*)(p + 20);
            // qDebug() << "已经在监控"<<code<<value;
            if (type != 1)
                continue; // EV_KEY
            if (code == 357 && value == 1) {
                if (keyCallback)
                    keyCallback("电源按键");
            }
            if (code == 355 && value == 1) {
                if (keyCallback)
                    keyCallback("录制按键");
            }
            if (code == 116 && value == 1) {
                if (keyCallback)
                    keyCallback("电源按键");
            }
            if (code == 126 && value == 1) {
                if (keyCallback)
                    keyCallback("翻转镜头按键");
            }
            if (code == 137 && value == 1) {
                if (keyCallback)
                    keyCallback("录制按键");
            }
        }
    });
    connect(keyProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                // qDebug() << "adb shell finished:";
                // USB拔插导致cat结束时，清理并延时重启
                keyProcess->kill();
                keyProcess->deleteLater();
                keyProcess = nullptr;
            });

    qDebug().noquote() << "ADB TX:" << QString::fromLatin1(cmd.toUtf8().toHex(' ').toUpper());
    keyProcess->start(cmd);
}

void Qadb::stopKeyMonitorAdbShell() {
    if (keyProcess) {
        keyProcess->kill();
        keyProcess->deleteLater();
        keyProcess = nullptr;
    }
}
