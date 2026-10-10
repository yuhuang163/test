#include "audio_expert_tcp.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QRegularExpression>
#include <QtEndian>

#if _MSC_VER >= 1600
#pragma execution_character_set("utf-8")
#endif

AudioExpertTcp::AudioExpertTcp() = default;

AudioExpertTcp::~AudioExpertTcp() {
    disconnectHost();
}

bool AudioExpertTcp::connectHost(const QString& host, int port, int timeoutMs, QString* errorMessage) {
    if (isConnected())
        disconnectHost();
    recvBuf_.clear();
    const QString ip = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    if (port <= 0 || port > 65535) {
        if (errorMessage)
            *errorMessage = QStringLiteral("端口无效");
        return false;
    }
    socket_.connectToHost(ip, static_cast<quint16>(port));
    if (!socket_.waitForConnected(qMax(200, timeoutMs))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("连接 AudioExpert 失败: %1").arg(socket_.errorString());
        return false;
    }
    return true;
}

void AudioExpertTcp::disconnectHost() {
    if (socket_.state() == QAbstractSocket::UnconnectedState)
        return;
    socket_.disconnectFromHost();
    if (socket_.state() != QAbstractSocket::UnconnectedState)
        socket_.waitForDisconnected(500);
    recvBuf_.clear();
}

bool AudioExpertTcp::isConnected() const {
    return socket_.state() == QAbstractSocket::ConnectedState;
}

bool AudioExpertTcp::writeFrame(const QByteArray& payload, int timeoutMs, QString* errorMessage) {
    if (!isConnected()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("未连接");
        return false;
    }
    QByteArray frame;
    frame.resize(4);
    qToBigEndian<qint32>(static_cast<qint32>(payload.size()), reinterpret_cast<uchar*>(frame.data()));
    frame.append(payload);
    if (socket_.write(frame) != frame.size()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("发送失败: %1").arg(socket_.errorString());
        return false;
    }
    if (!socket_.waitForBytesWritten(qMax(200, timeoutMs))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("发送超时");
        return false;
    }
    return true;
}

bool AudioExpertTcp::readFrame(QByteArray* payload, int timeoutMs, QString* errorMessage) {
    if (!payload) {
        if (errorMessage)
            *errorMessage = QStringLiteral("内部错误");
        return false;
    }
    QElapsedTimer timer;
    timer.start();
    while (!timer.hasExpired(timeoutMs)) {
        while (recvBuf_.size() < 4) {
            if (!socket_.waitForReadyRead(qMin(100, timeoutMs))) {
                if (timer.hasExpired(timeoutMs))
                    break;
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
                if (!isConnected()) {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("连接已断开");
                    return false;
                }
                continue;
            }
            recvBuf_.append(socket_.readAll());
        }
        if (recvBuf_.size() < 4) {
            if (errorMessage)
                *errorMessage = QStringLiteral("读长度超时");
            return false;
        }
        const qint32 len = qFromBigEndian<qint32>(reinterpret_cast<const uchar*>(recvBuf_.constData()));
        if (len < 0 || len > 16 * 1024 * 1024) {
            if (errorMessage)
                *errorMessage = QStringLiteral("非法帧长度: %1").arg(len);
            recvBuf_.clear();
            return false;
        }
        const int total = 4 + len;
        while (recvBuf_.size() < total) {
            if (!socket_.waitForReadyRead(qMin(100, timeoutMs))) {
                if (timer.hasExpired(timeoutMs))
                    break;
                QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 10);
                if (!isConnected()) {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("连接已断开");
                    return false;
                }
                continue;
            }
            recvBuf_.append(socket_.readAll());
        }
        if (recvBuf_.size() < total) {
            if (errorMessage)
                *errorMessage = QStringLiteral("读载荷超时");
            return false;
        }
        *payload = recvBuf_.mid(4, len);
        recvBuf_.remove(0, total);
        return true;
    }
    if (errorMessage)
        *errorMessage = QStringLiteral("读帧超时");
    return false;
}

bool AudioExpertTcp::sendCommand(const QString& command, int timeoutMs, QString* errorMessage) {
    const QString cmd = command.trimmed().isEmpty() ? QStringLiteral("$test#") : command.trimmed();
    return writeFrame(cmd.toUtf8(), timeoutMs, errorMessage);
}

static bool lineHasOkField(const QString& line, const QString& key) {
    // 匹配 Result:'OK' / Result:"OK" / Result:OK
    const QRegularExpression re(QStringLiteral("%1\\s*:\\s*['\"]?OK['\"]?").arg(QRegularExpression::escape(key)),
                                QRegularExpression::CaseInsensitiveOption);
    return re.match(line).hasMatch();
}

static QString extractFieldValue(const QString& line, const QString& key) {
    const QRegularExpression re(
        QStringLiteral("%1\\s*:\\s*['\"]?([^,'\"\\s}\\]]+)['\"]?").arg(QRegularExpression::escape(key)),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(line);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

static QString extractLabeledLineValue(const QString& block, const QString& label) {
    // 匹配 "Data Info: xxx" 整行冒号后全文（可含空格、中文）
    const QRegularExpression re(
        QStringLiteral("^\\s*%1\\s*:\\s*(.+)\\s*$").arg(QRegularExpression::escape(label)),
        QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
    const QRegularExpressionMatch m = re.match(block);
    return m.hasMatch() ? m.captured(1).trimmed() : QString();
}

static bool looksLikeUnitToken(const QString& token) {
    const QString t = token.trimmed();
    if (t.isEmpty())
        return false;
    static const QStringList kHints = {QStringLiteral("dB"), QStringLiteral("Hz"), QStringLiteral("%"),
                                       QStringLiteral("V"),  QStringLiteral("A"),  QStringLiteral("Ohm"),
                                       QStringLiteral("Ω"),  QStringLiteral("Pa"), QStringLiteral("deg"),
                                       QStringLiteral("°"),  QStringLiteral("rms"), QStringLiteral("THD"),
                                       QStringLiteral("ppm"), QStringLiteral("ms"), QStringLiteral("us"),
                                       QStringLiteral("kHz"), QStringLiteral("MHz")};
    for (const QString& h : kHints) {
        if (t.contains(h, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

static void splitMetricAndUnit(const QString& dataInfo, QString* metricName, QString* unit) {
    QString info = dataInfo.trimmed();
    QString metric = info;
    QString u;

    // Data Info: 波形参数测量 - dBrms
    const int dash = info.lastIndexOf(QStringLiteral(" - "));
    if (dash >= 0) {
        metric = info.mid(dash + 3).trimmed();
        if (metric.isEmpty())
            metric = info;
    }
    // 可选：名称(单位)
    const QRegularExpression paren(QStringLiteral("^(.+?)\\s*[\\(（]\\s*([^\\)）]+)\\s*[\\)）]\\s*$"));
    const QRegularExpressionMatch pm = paren.match(metric);
    if (pm.hasMatch()) {
        metric = pm.captured(1).trimmed();
        u = pm.captured(2).trimmed();
    }
    if (u.isEmpty()) {
        if (looksLikeUnitToken(metric))
            u = metric;
        else
            u = metric; // 无独立单位时，量名兼作单位展示（如 THD、dBrms）
    }
    if (metricName)
        *metricName = metric;
    if (unit)
        *unit = u;
}

static QVector<double> parseBraceNumbers(const QString& braceInner) {
    QVector<double> values;
    const QRegularExpression numRe(QStringLiteral("[-+]?(?:\\d+\\.?\\d*|\\.\\d+)(?:[eE][-+]?\\d+)?"));
    QRegularExpressionMatchIterator it = numRe.globalMatch(braceInner);
    while (it.hasNext()) {
        bool ok = false;
        const double v = it.next().captured().toDouble(&ok);
        if (ok)
            values.append(v);
    }
    return values;
}

static AudioExpertTcp::MeasureItem parseOneMeasureBlock(const QString& block) {
    AudioExpertTcp::MeasureItem item;
    item.transmitId = extractLabeledLineValue(block, QStringLiteral("Transmit Identification"));
    item.dataInfo = extractLabeledLineValue(block, QStringLiteral("Data Info"));
    item.testResult = extractLabeledLineValue(block, QStringLiteral("Test Result"));
    item.limitText = extractLabeledLineValue(block, QStringLiteral("Limit"));
    const QString unitLine = extractLabeledLineValue(block, QStringLiteral("Unit"));
    splitMetricAndUnit(item.dataInfo, &item.metricName, &item.unit);
    if (!unitLine.isEmpty())
        item.unit = unitLine;

    const QRegularExpression dataRe(QStringLiteral("Data\\s*:\\s*\\{([^}]*)\\}"),
                                    QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch dm = dataRe.match(block);
    if (dm.hasMatch()) {
        item.values = parseBraceNumbers(dm.captured(1));
        if (!item.values.isEmpty()) {
            item.value = item.values.first();
            item.hasValue = true;
        }
    }
    return item;
}

QVector<AudioExpertTcp::MeasureItem> AudioExpertTcp::parseMeasureItems(const QString& dataText) {
    QVector<MeasureItem> items;
    if (dataText.trimmed().isEmpty())
        return items;

    // 按 Transmit Identification 分块（首块可能无该行则整段当一块）
    const QRegularExpression splitRe(QStringLiteral("(?=^\\s*Transmit Identification\\s*:)"),
                                     QRegularExpression::CaseInsensitiveOption
                                         | QRegularExpression::MultilineOption);
    QStringList blocks = dataText.split(splitRe, Qt::SkipEmptyParts);
    if (blocks.isEmpty())
        blocks.append(dataText);

    for (const QString& raw : blocks) {
        const QString block = raw.trimmed();
        if (block.isEmpty())
            continue;
        // 跳过纯帧头、无实质字段的碎片
        if (!block.contains(QStringLiteral("Data"), Qt::CaseInsensitive)
            && !block.contains(QStringLiteral("Transmit Identification"), Qt::CaseInsensitive))
            continue;
        MeasureItem item = parseOneMeasureBlock(block);
        if (item.transmitId.isEmpty() && item.dataInfo.isEmpty() && !item.hasValue)
            continue;
        items.append(item);
    }
    return items;
}

QVector<AudioExpertTcp::MeasureItem> AudioExpertTcp::parseMeasureItems(const QStringList& dataChunks) {
    return parseMeasureItems(dataChunks.join(QLatin1Char('\n')));
}

bool AudioExpertTcp::waitTestOver(int timeoutMs, bool requireOk, RunResult* out, QString* errorMessage) {
    RunResult local;
    RunResult* r = out ? out : &local;
    QElapsedTimer timer;
    timer.start();
    while (!timer.hasExpired(timeoutMs)) {
        const int remain = static_cast<int>(qMax<qint64>(1, timeoutMs - timer.elapsed()));
        QByteArray payload;
        QString err;
        if (!readFrame(&payload, remain, &err)) {
            if (errorMessage)
                *errorMessage = err;
            return false;
        }
        const QString text = QString::fromUtf8(payload);
        // 兼容 #$^data^ / # $^data^ / $^data^
        if (text.contains(QStringLiteral("$^data^"))) {
            r->dataChunks.append(text);
        } else if (text.contains(QStringLiteral("$test_over"))) {
            r->testOverLine = text;
            r->result = extractFieldValue(text, QStringLiteral("Result"));
            r->status = extractFieldValue(text, QStringLiteral("Status"));
            r->measures = parseMeasureItems(r->dataChunks);
            if (requireOk) {
                const bool resultOk = lineHasOkField(text, QStringLiteral("Result"));
                const bool statusOk = lineHasOkField(text, QStringLiteral("Status"));
                if (!resultOk || !statusOk) {
                    r->pass = false;
                    r->failReason = QStringLiteral("test_over 未通过: %1").arg(text);
                    if (errorMessage)
                        *errorMessage = r->failReason;
                    return false;
                }
            }
            r->pass = true;
            return true;
        } else if (r->ackLine.isEmpty() && text.contains(QStringLiteral("OK"))) {
            r->ackLine = text;
        }
    }
    if (errorMessage)
        *errorMessage = QStringLiteral("等待 $test_over 超时");
    r->pass = false;
    r->failReason = QStringLiteral("等待 $test_over 超时");
    r->measures = parseMeasureItems(r->dataChunks);
    return false;
}

bool AudioExpertTcp::runTest(const QString& host,
                             int port,
                             const QString& command,
                             int connectTimeoutMs,
                             int readTimeoutMs,
                             bool requireOk,
                             RunResult* out,
                             QString* errorMessage) {
    AudioExpertTcp client;
    QString err;
    if (!client.connectHost(host, port, connectTimeoutMs, &err)) {
        if (errorMessage)
            *errorMessage = err;
        return false;
    }
    if (!client.sendCommand(command, qMin(connectTimeoutMs, 5000), &err)) {
        if (errorMessage)
            *errorMessage = err;
        client.disconnectHost();
        return false;
    }
    const bool ok = client.waitTestOver(readTimeoutMs, requireOk, out, &err);
    client.disconnectHost();
    if (!ok && errorMessage)
        *errorMessage = err;
    return ok;
}
