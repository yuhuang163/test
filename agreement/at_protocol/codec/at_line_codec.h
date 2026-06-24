#ifndef AT_LINE_CODEC_H
#define AT_LINE_CODEC_H

#include <QByteArray>
#include <QString>
#include <QQueue>
#include <functional>

struct AtFrame {
    QString cmd;
    QString parameter;
};

class AtLineCodec {
public:
    using FrameHandler = std::function<void(const AtFrame& frame)>;

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

#endif // AT_LINE_CODEC_H
