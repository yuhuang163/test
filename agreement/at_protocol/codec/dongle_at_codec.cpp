#include "dongle_at_codec.h"
#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

void DongleAtCodec::reset() {
    state_ = State::Idle;
    cmd_.clear();
    parameter_.clear();
    dataQueue_.clear();
    cangonext_ = 0;
}

bool DongleAtCodec::isPrintableAtLine(const QString& line) const {
    if (!line.startsWith(QStringLiteral("AT"))) {
        return false;
    }
    for (const QChar& ch : line) {
        const ushort u = ch.unicode();
        if (u == '\r' || u == '\n' || u == '\t') {
            continue;
        }
        if (u < 0x20 || u > 0x7E) {
            return false;
        }
    }
    return true;
}

void DongleAtCodec::feed(const QByteArray& chunk, const FrameHandler& onFrame) {
    for (char c : chunk) {
        dataQueue_.push_back(c);
    }

    while (!dataQueue_.isEmpty()) {
        char c = dataQueue_.dequeue();

        switch (state_) {
        case State::Idle:
            if (c == 'A') {
                cmd_ += c;
                state_ = State::ReceivingT;
            }
            break;

        case State::ReceivingT:
            if (c == 'T') {
                cmd_ += c;
                state_ = State::ReceivingCommand;
            } else {
                cmd_.clear();
                state_ = State::Idle;
            }
            break;

        case State::ReceivingCommand:
            if (c == '\r') {
                cangonext_ = 1;
            } else if (cangonext_ && c == '\n') {
                cangonext_ = 0;
                const QString atLine = parameter_.isEmpty() ? cmd_ + "\r\n" : cmd_ + "=" + parameter_ + "\r\n";
                if (isPrintableAtLine(atLine)) {
                    qDebug().noquote() << "AT RX:" << atLine.trimmed();
                    if (onFrame) onFrame({cmd_, parameter_});
                }
                cmd_.clear();
                parameter_.clear();
                state_ = State::Idle;
            } else if (c == '=') {
                state_ = State::ReceivingParameter;
            } else {
                cmd_ += c;
                if (cmd_.size() > 1024) {
                    cmd_.clear();
                    state_ = State::Idle;
                }
            }
            break;

        case State::ReceivingParameter:
            if (c == '\r') {
                cangonext_ = 1;
            } else if (cangonext_ && c == '\n') {
                cangonext_ = 0;
                const QString atLine = parameter_.isEmpty() ? cmd_ + "\r\n" : cmd_ + "=" + parameter_ + "\r\n";
                if (isPrintableAtLine(atLine)) {
                    qDebug().noquote() << "AT RX:" << atLine.trimmed();
                    if (onFrame) onFrame({cmd_, parameter_});
                }
                cmd_.clear();
                parameter_.clear();
                state_ = State::Idle;
            } else {
                parameter_ += c;
                if (parameter_.size() > 1024) {
                    parameter_.clear();
                    state_ = State::Idle;
                }
            }
            break;
        }
    }
}
