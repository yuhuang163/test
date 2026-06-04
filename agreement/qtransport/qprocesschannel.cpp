#include "qprocesschannel.h"

#include <QDateTime>
#include <QDebug>

QProcessChannel::QProcessChannel(QObject* parent) : QObject(parent) {
    process_ = new QProcess(this);
    process_->setProcessChannelMode(QProcess::MergedChannels);
    connect(process_, &QProcess::readyReadStandardOutput, this, &QProcessChannel::onReadyRead);
    connect(process_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            &QProcessChannel::onFinished);

    connect(&timeoutTimer_, &QTimer::timeout, this, &QProcessChannel::checkTimeout);
    timeoutTimer_.start(500);
}

QProcessChannel::~QProcessChannel() {
    stop();
}

bool QProcessChannel::start(const QString& program, const QStringList& args, int startTimeoutMs) {
    program_ = program;
    args_ = args;
    startTimeoutMs_ = startTimeoutMs;
    if (process_->state() != QProcess::NotRunning) {
        return true;
    }
    process_->start(program, args);
    if (!process_->waitForStarted(startTimeoutMs)) {
        qDebug() << "QProcessChannel start failed:" << program << args;
        return false;
    }
    if (!initCommand_.isEmpty()) {
        qDebug().noquote() << txPrefix_ << QString::fromLatin1(initCommand_.toHex(' ').toUpper());
        process_->write(initCommand_);
    }
    return true;
}

void QProcessChannel::stop(const QString& exitCommand) {
    if (process_ && process_->state() != QProcess::NotRunning) {
        const QByteArray cmd = (exitCommand + QLatin1Char('\n')).toUtf8();
        qDebug().noquote() << txPrefix_ << QString::fromLatin1(cmd.toHex(' ').toUpper());
        process_->write(cmd);
        process_->waitForFinished(1000);
    }
    queue_.clear();
    buffer_.clear();
    isProcessing_ = false;
}

void QProcessChannel::setInitCommand(const QByteArray& initCommand) {
    initCommand_ = initCommand;
}

void QProcessChannel::setTxPrefix(const QString& prefix) {
    txPrefix_ = prefix;
}

void QProcessChannel::setRxPrefix(const QString& prefix) {
    rxPrefix_ = prefix;
}

void QProcessChannel::setRestartOnTimeout(bool enabled, int restartStartTimeoutMs) {
    restartOnTimeout_ = enabled;
    restartStartTimeoutMs_ = restartStartTimeoutMs;
}

void QProcessChannel::sendCommand(const QString& cmd, Callback callback, qint64 timeoutMs) {
    CmdItem item;
    item.command = cmd;
    item.endMark = buildEndMark();
    item.timer.start();
    item.callback = std::move(callback);
    item.timeoutMs = timeoutMs;
    queue_.enqueue(item);
    if (!isProcessing_) {
        processNextCommand();
    }
}

void QProcessChannel::onReadyRead() {
    const QByteArray output = process_->readAllStandardOutput();
    if (output.isEmpty()) {
        return;
    }
    qDebug().noquote() << rxPrefix_ << QString::fromLatin1(output.toHex(' ').toUpper());
    buffer_ += QString::fromUtf8(output);

    if (queue_.isEmpty()) {
        return;
    }

    CmdItem item = queue_.head();
    const int pos = buffer_.indexOf(item.endMark);
    if (pos < 0) {
        return;
    }
    const QString result = buffer_.left(pos).trimmed();
    buffer_.remove(0, pos + item.endMark.length());
    queue_.dequeue();
    processNextCommand();
    if (item.callback) {
        item.callback(result, item.timer.elapsed());
    }
}

void QProcessChannel::onFinished(int, QProcess::ExitStatus) {
    while (!queue_.isEmpty()) {
        CmdItem item = queue_.dequeue();
        if (item.callback) {
            item.callback(QStringLiteral("进程已退出"), item.timer.elapsed());
        }
    }
    buffer_.clear();
    isProcessing_ = false;
}

void QProcessChannel::checkTimeout() {
    if (queue_.isEmpty()) {
        return;
    }
    CmdItem& item = queue_.head();
    if (item.timer.elapsed() <= item.timeoutMs) {
        return;
    }
    if (item.callback) {
        item.callback(QStringLiteral("命令超时"), item.timer.elapsed());
    }
    queue_.dequeue();
    buffer_.clear();
    isProcessing_ = false;
    if (restartOnTimeout_) {
        restartProcess();
    }
    processNextCommand();
}

void QProcessChannel::processNextCommand() {
    if (queue_.isEmpty()) {
        isProcessing_ = false;
        return;
    }
    isProcessing_ = true;
    CmdItem& item = queue_.head();
    item.timer.restart();
    const QString command = item.command + QStringLiteral(" ; echo ") + item.endMark;
    const QByteArray data = (command + QLatin1Char('\n')).toUtf8();
    qDebug().noquote() << txPrefix_ << QString::fromLatin1(data.toHex(' ').toUpper());
    process_->write(data);
}

QString QProcessChannel::buildEndMark() {
    return QStringLiteral("__CMD_END_%1__").arg(QDateTime::currentMSecsSinceEpoch());
}

bool QProcessChannel::restartProcess() {
    if (program_.isEmpty()) {
        return false;
    }
    if (process_->state() != QProcess::NotRunning) {
        process_->kill();
        process_->waitForFinished(1000);
    }
    process_->start(program_, args_);
    if (!process_->waitForStarted(restartStartTimeoutMs_)) {
        qDebug() << "QProcessChannel restart failed:" << program_ << args_;
        return false;
    }
    if (!initCommand_.isEmpty()) {
        qDebug().noquote() << txPrefix_ << QString::fromLatin1(initCommand_.toHex(' ').toUpper());
        process_->write(initCommand_);
    }
    return true;
}
