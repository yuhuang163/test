#ifndef DONGLE_AT_CODEC_H
#define DONGLE_AT_CODEC_H

#include <QByteArray>
#include <QString>
#include <QQueue>
#include <functional>

struct DongleAtFrame {
    QString cmd;
    QString parameter;
};

class DongleAtCodec {
public:
    using FrameHandler = std::function<void(const DongleAtFrame& frame)>;

    void reset();
    void feed(const QByteArray& chunk, const FrameHandler& onFrame);

private:
    bool isPrintableAtLine(const QString& line) const;

    enum class State {
        Idle,
        ReceivingT,
        ReceivingCommand,
        ReceivingParameter
    };
    State state_ = State::Idle;
    QString cmd_;
    QString parameter_;
    QQueue<char> dataQueue_;
    int cangonext_ = 0;
};

#endif // DONGLE_AT_CODEC_H
