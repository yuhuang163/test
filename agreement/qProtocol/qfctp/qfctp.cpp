#include "qfctp.h"
#include <QDebug>
#include <QString>
#include "comm_protocol_parser.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8"
#endif

static void qfctp_on_full_frame(const uint8_t *frame_data, uint16_t frame_len, void *user_data)
{
    auto *self = static_cast<Qfctp *>(user_data);
    (void)self;
    comm_protocol_frame_node_t frame{};
    const int rc = comm_protocol_frame_deserialize(frame_data, frame_len, &frame);
    if (rc != COMM_PROTOCOL_ERROR_NONE) {
        qWarning() << "FCTP 帧反序列化失败:" << rc;
        return;
    }
    qDebug() << "FCTP 帧 seq" << frame.seq << "type" << frame.frame_type
             << "payload" << frame.payload_length << "services"
             << frame.payload.service_count;
}

Qfctp::Qfctp(QSerialPort* parent) : QSerialPort(parent) {}

void Qfctp::parseCmd(const QByteArray& byte) {
    if (byte.isEmpty()) {
        return;
    }
    const auto *p = reinterpret_cast<const uint8_t *>(byte.constData());
    const int  rc = comm_protocol_assemble_packet(p, static_cast<uint16_t>(byte.size()), qfctp_on_full_frame, this);
    if (rc < 0) {
        qWarning() << "comm_protocol_assemble_packet 错误:" << rc;
    }
}

void Qfctp::set(DeviceCmd cmd, const QVariant& data) {
    (void)data;
    qWarning() << "Qfctp 暂未实现 set 命令，cmd =" << static_cast<int>(cmd);
}

void Qfctp::get(DeviceCmd cmd, const QVariant& param) {
    (void)param;
    qWarning() << "Qfctp 暂未实现 get 命令，cmd =" << static_cast<int>(cmd);
}
