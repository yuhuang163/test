#include "press_fixture_device.h"

#include <QDateTime>
#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

PressFixtureDevice::PressFixtureDevice(WriteFn writeFn, DelayFn delayFn)
    : write_(std::move(writeFn)), delay_(std::move(delayFn)) {
    protocol_.initCommands();
}

void PressFixtureDevice::sendFixtureState(FixtureState state) {
    const QByteArray frame = FixturePressUartProtocol::buildFixtureStateCommand(state);
    write_(frame, false, false);
}

void PressFixtureDevice::sendCommand(int commandId, int numb) {
    if (!protocol_.isValidCommand(commandId, numb)) {
        qDebug() << "Invalid press command - command_id:" << commandId << "numb:" << numb;
        return;
    }

    const qint64 current_timestamp = QDateTime::currentDateTime().toMSecsSinceEpoch();
    if (current_timestamp - last_sent_timestamp_ < 100 && last_sent_timestamp_ != 0) {
        qDebug() << "press short interval:" << current_timestamp - last_sent_timestamp_;
        if (delay_)
            delay_(100);
    }
    last_sent_timestamp_ = QDateTime::currentDateTime().toMSecsSinceEpoch();
    last_commid_timestamp_ = last_sent_timestamp_;

    QByteArray frame;
    if (!protocol_.commandBytes(commandId, numb, &frame)) {
        qDebug() << "Invalid press command index";
        return;
    }
    qDebug() << "press command_id:" << commandId << "numb" << numb;
    write_(frame, true, false);
    qDebug() << "已发送压感治具命令" << frame;
}

FixturePressUartEvent PressFixtureDevice::parseReceived(const QByteArray& data) const {
    return FixturePressUartProtocol::parseReceived(data);
}

qint64 PressFixtureDevice::lastCommidTimestamp() const {
    return last_commid_timestamp_;
}

void PressFixtureDevice::setLastCommidTimestamp(qint64 timestamp) {
    last_commid_timestamp_ = timestamp;
}

machine_command_id_e PressFixtureDevice::lastCommid() const {
    return last_commid_;
}

void PressFixtureDevice::setLastCommid(machine_command_id_e commandId) {
    last_commid_ = commandId;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
