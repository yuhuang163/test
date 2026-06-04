#include "qaiot.h"

#include <QDebug>
#include <QStringList>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {
QString hexText(const QByteArray& data) {
    return QString::fromLatin1(data.toHex(' ').toUpper());
}

bool toByteValue(const QVariant& v, quint8* out) {
    bool ok = false;
    const int n = v.toInt(&ok);
    if (!ok || n < 0 || n > 0xFF) {
        return false;
    }
    if (out) {
        *out = static_cast<quint8>(n);
    }
    return true;
}
}  // namespace

Qaiot::Qaiot(QSerialPort* parent) : qProtocol(parent), serialPort(parent) {}

void Qaiot::parseCmd(const QByteArray& byte) {
    if (byte.size() < 2) {
        return;
    }

    Message message;
    QString errorMessage;
    if (!parseMessage(byte, &message, &errorMessage)) {
        qDebug() << "QAIOT 解析失败:" << errorMessage;
        return;
    }

    qDebug().noquote() << "QAIOT RX:" << hexText(byte);
    emit send_pb_date(QStringLiteral("QAIOT RX %1").arg(describeMessage(message)));
}

void Qaiot::set(DeviceCmd cmd, const QVariant& data) {
    Q_UNUSED(data);
    emit send_pb_date(QStringLiteral("QAIOT 暂未映射 set 命令，cmd=%1").arg(static_cast<int>(cmd)));
}

void Qaiot::get(DeviceCmd cmd, const QVariant& param) {
    Q_UNUSED(param);
    emit send_pb_date(QStringLiteral("QAIOT 暂未映射 get 命令，cmd=%1").arg(static_cast<int>(cmd)));
}

bool Qaiot::sendCustomMessage(const QVariantMap& map) {
    quint8 serviceId = 0;
    quint8 commandId = 0;
    if (!toByteValue(map.value(QStringLiteral("serviceId")), &serviceId) ||
        !toByteValue(map.value(QStringLiteral("commandId")), &commandId)) {
        qWarning() << "QAIOT 通用发送缺少或非法 serviceId/commandId";
        return false;
    }

    QList<TlvNode> tlvs;
    QString errorMessage;
    const QVariant tlvListValue = map.value(QStringLiteral("tlvs"));
    if (tlvListValue.isValid()) {
        const QVariantList list = tlvListValue.toList();
        for (const QVariant& item : list) {
            TlvNode node;
            if (!tlvFromVariant(item, &node, &errorMessage)) {
                qWarning() << "QAIOT TLV 参数非法:" << errorMessage;
                return false;
            }
            tlvs.append(node);
        }
    } else if (map.contains(QStringLiteral("type"))) {
        TlvNode node;
        if (!tlvFromVariant(map, &node, &errorMessage)) {
            qWarning() << "QAIOT TLV 参数非法:" << errorMessage;
            return false;
        }
        tlvs.append(node);
    }

    const QByteArray frame = buildMessage(serviceId, commandId, tlvs);
    if (!serialPort || !serialPort->isOpen()) {
        qWarning() << "QAIOT 串口未打开，未发送数据";
        return false;
    }
    const qint64 n = serialPort->write(frame);
    qDebug().noquote() << "QAIOT TX:" << hexText(frame);
    if (n != frame.size()) {
        qWarning() << "QAIOT 发送不完整" << n << "/" << frame.size();
        return false;
    }
    return true;
}

bool Qaiot::parseMessage(const QByteArray& frame, Message* message, QString* errorMessage) const {
    if (!message) {
        return false;
    }
    if (frame.size() < 2) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("报文长度不足");
        }
        return false;
    }
    message->serviceId = static_cast<quint8>(frame.at(0));
    message->commandId = static_cast<quint8>(frame.at(1));
    message->tlvs.clear();
    return parseTlvs(frame, 2, frame.size(), &message->tlvs, errorMessage);
}

bool Qaiot::parseTlvs(const QByteArray& data, int start, int end, QList<TlvNode>* out, QString* errorMessage) const {
    if (!out || start < 0 || end > data.size() || start > end) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TLV 范围非法");
        }
        return false;
    }

    int pos = start;
    while (pos < end) {
        TlvNode node;
        node.rawType = static_cast<quint8>(data.at(pos++));
        node.hasChildren = (node.rawType & 0x80u) != 0;
        node.type = node.rawType & 0x7Fu;

        int length = 0;
        if (!readVarLength(data, &pos, end, &length, errorMessage)) {
            return false;
        }
        if (pos + length > end) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("TLV 长度越界 type=%1 length=%2").arg(node.type).arg(length);
            }
            return false;
        }

        if (node.hasChildren && length > 0) {
            if (!parseTlvs(data, pos, pos + length, &node.children, errorMessage)) {
                return false;
            }
        } else {
            node.value = data.mid(pos, length);
        }
        pos += length;
        out->append(node);
    }
    return true;
}

bool Qaiot::readVarLength(const QByteArray& data, int* pos, int end, int* length, QString* errorMessage) const {
    if (!pos || !length || *pos >= end) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Length 字段缺失");
        }
        return false;
    }

    const quint8 b0 = static_cast<quint8>(data.at((*pos)++));
    if ((b0 & 0x80u) == 0) {
        *length = b0;
        return true;
    }

    if (*pos >= end) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Length 第二字节缺失");
        }
        return false;
    }
    const quint8 b1 = static_cast<quint8>(data.at((*pos)++));
    if ((b1 & 0x80u) != 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Length 超过当前支持的 2 字节范围");
        }
        return false;
    }
    *length = ((b0 & 0x7F) << 7) | (b1 & 0x7F);
    return true;
}

QByteArray Qaiot::buildMessage(quint8 serviceId, quint8 commandId, const QList<TlvNode>& tlvs) const {
    QByteArray frame;
    frame.append(static_cast<char>(serviceId));
    frame.append(static_cast<char>(commandId));
    frame.append(buildTlvs(tlvs));
    return frame;
}

QByteArray Qaiot::buildTlvs(const QList<TlvNode>& tlvs) const {
    QByteArray out;
    for (const TlvNode& tlv : tlvs) {
        QByteArray payload = tlv.hasChildren ? buildTlvs(tlv.children) : tlv.value;
        quint8 rawType = tlv.type & 0x7Fu;
        if (tlv.hasChildren && !payload.isEmpty()) {
            rawType |= 0x80u;
        }
        out.append(static_cast<char>(rawType));
        out.append(encodeVarLength(payload.size()));
        out.append(payload);
    }
    return out;
}

QByteArray Qaiot::encodeVarLength(int length) const {
    QByteArray out;
    if (length < 0 || length > 0x3FFF) {
        return out;
    }
    if (length <= 0x7F) {
        out.append(static_cast<char>(length & 0x7F));
        return out;
    }
    out.append(static_cast<char>(0x80u | ((length >> 7) & 0x7F)));
    out.append(static_cast<char>(length & 0x7F));
    return out;
}

bool Qaiot::tlvFromVariant(const QVariant& value, TlvNode* out, QString* errorMessage) const {
    if (!out) {
        return false;
    }
    const QVariantMap map = value.toMap();
    if (map.isEmpty()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TLV 需要 QVariantMap");
        }
        return false;
    }

    quint8 type = 0;
    if (!toByteValue(map.value(QStringLiteral("type")), &type) || type > 0x7F) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("TLV type 非法");
        }
        return false;
    }
    out->type = type;
    out->children.clear();
    out->value.clear();

    const QVariant childrenValue = map.value(QStringLiteral("children"));
    if (childrenValue.isValid()) {
        out->hasChildren = true;
        const QVariantList children = childrenValue.toList();
        for (const QVariant& childValue : children) {
            TlvNode child;
            if (!tlvFromVariant(childValue, &child, errorMessage)) {
                return false;
            }
            out->children.append(child);
        }
        return true;
    }

    out->hasChildren = map.value(QStringLiteral("hasChildren"), false).toBool();
    if (!valueFromVariant(map.value(QStringLiteral("value")), &out->value, errorMessage)) {
        return false;
    }
    return true;
}

bool Qaiot::valueFromVariant(const QVariant& value, QByteArray* out, QString* errorMessage) const {
    if (!out) {
        return false;
    }
    if (!value.isValid() || value.isNull()) {
        out->clear();
        return true;
    }
    if (value.type() == QVariant::ByteArray) {
        *out = value.toByteArray();
        return true;
    }
    const QString text = value.toString().trimmed();
    QString hex = text;
    if (hex.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        hex = hex.mid(2);
    }
    hex.remove(QLatin1Char(' '));
    hex.remove(QLatin1Char(':'));
    if (!hex.isEmpty() && hex.size() % 2 == 0) {
        bool isHex = true;
        for (const QChar& ch : hex) {
            const ushort upper = ch.toUpper().unicode();
            if (!ch.isDigit() && (upper < 'A' || upper > 'F')) {
                isHex = false;
                break;
            }
        }
        if (isHex) {
            *out = QByteArray::fromHex(hex.toLatin1());
            return true;
        }
    }
    if (text.isEmpty() && value.toString().isEmpty()) {
        out->clear();
        return true;
    }
    if (errorMessage) {
        *errorMessage = QStringLiteral("TLV value 需 QByteArray 或偶数字节 hex 字符串");
    }
    return false;
}

QString Qaiot::describeMessage(const Message& message) const {
    QStringList parts;
    for (const TlvNode& tlv : message.tlvs) {
        parts << describeTlv(tlv);
    }
    return QStringLiteral("service=%1 command=%2 tlvs=[%3]")
        .arg(message.serviceId)
        .arg(message.commandId)
        .arg(parts.join(QStringLiteral("; ")));
}

QString Qaiot::describeTlv(const TlvNode& tlv, int depth) const {
    Q_UNUSED(depth);
    if (tlv.hasChildren) {
        QStringList children;
        for (const TlvNode& child : tlv.children) {
            children << describeTlv(child, depth + 1);
        }
        return QStringLiteral("type=%1 children={%2}").arg(tlv.type).arg(children.join(QStringLiteral(", ")));
    }
    return QStringLiteral("type=%1 len=%2 value=%3")
        .arg(tlv.type)
        .arg(tlv.value.size())
        .arg(hexText(tlv.value));
}
