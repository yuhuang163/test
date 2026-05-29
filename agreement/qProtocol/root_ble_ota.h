#ifndef ROOT_BLE_OTA_H
#define ROOT_BLE_OTA_H

#include <QByteArray>
#include <QQueue>
#include <QString>
#include <QtGlobal>
#include <functional>

/** 路特 TLV BLE OTA（见 docs/基于 TLV 的 BLE OTA 图片资源升级协议.md） */
class RootBleOtaClient {
public:
    using CancelPredicate = std::function<bool()>;
    using SendFunc = std::function<void(const QByteArray&)>;
    using LogFunc = std::function<void(const QString&)>;

    static constexpr uint8_t kServerId = 0x06;                    // OTA 业务 Server ID，写入 Payload 首字节。
    static constexpr uint8_t kServerIdLen = 0x01;                 // Server ID 固定 1 字节。
    static constexpr uint32_t kImageIdUiResource = 0x00000006u;   // 图片资源升级目标 ID。
    static constexpr int kDefaultFragmentSize = 200;              // BLOCK_DATA 单帧默认分片长度。
    static constexpr int kDefaultSuggestBlockSize = 4096;         // 协商块大小时给设备的建议值。
    static constexpr int kBlockCompleteTimeoutMs = 500;           // 等待单块完成 ACK/NACK 的超时时间。
    static constexpr int kResponseTimeoutMs = 30000;              // 协商、开始、结束等控制命令响应超时。
    static constexpr int kMaxRetry = 3;                           // 单块失败后的最大整块重发次数。
    static constexpr int kDefaultBlockBusyWaitMs = 500;           // 设备忙 NACK 后默认退避等待（ms）。

    enum TlvType : uint8_t {
        NegotiateBsReq = 0x01,
        NegotiateBsResp = 0x02,
        StartOtaReq = 0x03,
        StartOtaResp = 0x04,
        BlockData = 0x05,
        BlockComplete = 0x06,
        BlockCompleteAck = 0x07,
        EndOtaReq = 0x08,
        EndOtaResp = 0x09,
        Nack = 0x0A,
    };

    struct TlvMessage {
        uint8_t type = 0;
        QByteArray value;
    };

    void setSendFunc(SendFunc fn) { sendFunc_ = std::move(fn); }
    void setLogFunc(LogFunc fn) { logFunc_ = std::move(fn); }
    void reset();
    void onRx(const QByteArray& data);
    bool tryTakeResponse(uint8_t type, QByteArray* outValue);

    using ProgressFunc = std::function<void(int percent)>;

    /** 执行完整 OTA；intervalMs 为片段发送间隔，blockBusyWaitMs 为设备忙退避等待 */
    bool runTransfer(const QByteArray& imageData, uint32_t imageId, uint32_t version, int intervalMs,
                     int blockBusyWaitMs, CancelPredicate cancelled, QString* errorOut,
                     ProgressFunc onProgress = nullptr);

    static uint32_t calculateImageCrc32(const QByteArray& imageData);
    static QByteArray buildFrame(uint8_t& seq, uint8_t frameType, const QByteArray& payload);
    static QByteArray buildOtaPayload(uint8_t tlvType, const QByteArray& tlvValue);

private:
    enum class BlockSendResult {
        Success,
        RetryBlock,
        RetryBlockAfterBackoff,
        Failed,
    };

    void drainRxBuffer();
    bool parseOneFrame(const QByteArray& frame);
    bool sendTlvRequest(uint8_t tlvType, const QByteArray& tlvValue);
    bool waitTlv(uint8_t tlvType, int timeoutMs, QByteArray* outValue, CancelPredicate cancelled);
    bool negotiateBlockSize(int* outBlockSize, CancelPredicate cancelled, QString* errorOut);
    bool startOtaSession(uint32_t imageId, uint32_t version, int blockSize, const QByteArray& imageData,
                       int* outNextBlock, CancelPredicate cancelled, QString* errorOut);
    BlockSendResult sendBlock(int blockNumber, int blockSize, const QByteArray& imageData, int intervalMs,
                              CancelPredicate cancelled, QString* errorOut, ProgressFunc onProgress);
    bool endOtaSession(uint32_t imageId, CancelPredicate cancelled, QString* errorOut);

    SendFunc sendFunc_;
    LogFunc logFunc_;
    QByteArray rxBuffer_;
    QQueue<TlvMessage> responses_;
    uint8_t seq_ = 0;
};

#endif  // ROOT_BLE_OTA_H
