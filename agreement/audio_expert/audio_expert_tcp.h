#ifndef AUDIO_EXPERT_TCP_H
#define AUDIO_EXPERT_TCP_H

#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QVector>

/**
 * AudioExpert Remote TCP 客户端（见 docs/开发参考资料/AudioExpert_TCP通讯说明.pdf）。
 * 帧格式：4 字节大端 I32 长度 + UTF-8/ASCII 载荷。
 */
class AudioExpertTcp {
public:
    /** 一条远程结果传输块（#$^data^ 内一段 Transmit Identification…）。 */
    struct MeasureItem {
        QString transmitId; // Transmit_0
        QString dataInfo;   // 波形参数测量 - dBrms
        QString metricName; // dBrms / THD 等短名
        QString unit;       // 单位（无独立 Unit 行时常用 metricName）
        QString testResult; // Not Evaluated / Pass / …
        QString limitText;  // None 或限值原文
        QVector<double> values;
        double value = 0; // 首个数值
        bool hasValue = false;
    };

    struct RunResult {
        bool pass = false;
        QString failReason;
        QString ackLine;      // 如 $test^0^OK#$
        QString testOverLine; // 含 $test_over^...
        QString result;       // test_over 内 Result 字段
        QString status;       // test_over 内 Status 字段
        QStringList dataChunks;
        QVector<MeasureItem> measures; // 从 data 帧解析
    };

    AudioExpertTcp();
    ~AudioExpertTcp();

    bool connectHost(const QString& host, int port, int timeoutMs, QString* errorMessage = nullptr);
    void disconnectHost();
    bool isConnected() const;

    /** 发送一条指令（自动加长度前缀），默认文档示例 $test# */
    bool sendCommand(const QString& command, int timeoutMs, QString* errorMessage = nullptr);

    /**
     * 收帧直到出现 $test_over 或超时。
     * requireOk=true 时要求 test_over 行含 Result:'OK' 与 Status:'OK'（不区分大小写引号内）。
     */
    bool waitTestOver(int timeoutMs, bool requireOk, RunResult* out, QString* errorMessage = nullptr);

    /** 连接 → 发 command → 等到 test_over → 断开 */
    static bool runTest(const QString& host,
                        int port,
                        const QString& command,
                        int connectTimeoutMs,
                        int readTimeoutMs,
                        bool requireOk,
                        RunResult* out,
                        QString* errorMessage = nullptr);

    /** 解析 data 帧文本（可多块 Transmit Identification）。 */
    static QVector<MeasureItem> parseMeasureItems(const QString& dataText);
    static QVector<MeasureItem> parseMeasureItems(const QStringList& dataChunks);

private:
    bool writeFrame(const QByteArray& payload, int timeoutMs, QString* errorMessage);
    bool readFrame(QByteArray* payload, int timeoutMs, QString* errorMessage);

    QTcpSocket socket_;
    QByteArray recvBuf_;
};

/** 自由工站卡控 / MES：AudioExpert TCP 一轮测试结果。 */
struct ProtocolAudioExpertData {
    int pass = 0; // 1=通过 0=失败
    QString result;
    QString status;
    QString ackLine;
    QString testOverLine;
    QString dataText; // data 帧原文拼接

    int itemCount = 0;
    QString transmitId;
    QString itemsSummary; // 如 dBrms=-3.013519; THD=0.227627

    // 前 4 条解析项（便于卡控勾选；更多项见 itemsSummary / measures）
    double value = 0;
    double value2 = 0;
    double value3 = 0;
    double value4 = 0;
    QString unit;
    QString unit2;
    QString unit3;
    QString unit4;
    QString metricName;
    QString metricName2;
    QString metricName3;
    QString metricName4;

    QVector<AudioExpertTcp::MeasureItem> measures;
};

Q_DECLARE_METATYPE(ProtocolAudioExpertData)

#endif // AUDIO_EXPERT_TCP_H
