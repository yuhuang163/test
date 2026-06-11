#ifndef ROOT_BLE_OTA2_H
#define ROOT_BLE_OTA2_H

#include <QByteArray>
#include <QHash>
#include <QQueue>
#include <QString>
#include <QtGlobal>
#include <functional>

/** 路特 BLE OTA v2：图片资源原子 TLV（见 docs/IMG_RES_OTA_PROTOCOL.md） */
class RootBleOta2Client {
  public:
    using CancelPredicate = std::function<bool()>;
    using SendFunc = std::function<void(const QByteArray&)>;
    using LogFunc = std::function<void(const QString&)>;
    using ProgressFunc = std::function<void(int percent)>;

    static constexpr uint16_t kOtaServiceId = 9;
    static constexpr uint16_t kTestServiceId = 7;
    static constexpr uint32_t kDefaultImageId = 0x00000001u;
    static constexpr uint32_t kDefaultImageVersion = 0x00010000u; // TLV 0x0002 VERSION
    static constexpr int kBlockSize = 3848;
    static constexpr int kDefaultUartChunkSize = 128;      // 串口透传单包默认长度（与 OTA v1 分片 UI 一致）
    static constexpr int kUpgradeControlTimeoutMs = 10000; // 开始/结束升级等控制命令应答
    static constexpr int kBlockResponseTimeoutMs = 2000;   // 写数据块应答
    static constexpr int kMaxWriteRetry = 3;

    enum TlvId : uint16_t {
        ImgId = 0x0001,
        Version = 0x0002,
        Crc32 = 0x0003,
        TotalSize = 0x0004,
        Type = 0x0005,
        BlockSize = 0x0006,
        StartUpgradeRequest = 0x0007,
        ImgOffsetAddr = 0x0008,
        WriteImgData = 0x0009,
        EndUpgradeRequest = 0x000A,
        UpgradeResult = 0x000B,
        UpgradeStatusNotify = 0x000C,
        ServiceErrorCode = 0xF000,
    };

    struct FrameResponse {
        uint8_t seq = 0;
        QHash<uint16_t, QByteArray> tlvs;
    };

    void setSendFunc(SendFunc fn) {
        sendFunc_ = std::move(fn);
    }
    void setLogFunc(LogFunc fn) {
        logFunc_ = std::move(fn);
    }
    void reset();
    void onRx(const QByteArray& data);

    bool runTransfer(const QByteArray& imageData, uint32_t imageId, uint32_t version, int intervalMs,
                     int uartChunkSize, CancelPredicate cancelled, QString* errorOut,
                     ProgressFunc onProgress = nullptr);

    static uint32_t calculateImageCrc32(const QByteArray& imageData);

  private:
    static QByteArray appendTlvU32(uint16_t type, uint32_t value);
    static QByteArray appendTlvU8(uint16_t type, uint8_t value);
    static QByteArray appendTlvU16(uint16_t type, uint16_t value);
    static QByteArray appendTlvEmpty(uint16_t type);
    static QByteArray appendTlvBytes(uint16_t type, const QByteArray& value);
    static QByteArray buildOtaServicePayload(const QByteArray& tlvBlob);
    static QByteArray buildFrame(uint8_t& seq, uint8_t frameType, const QByteArray& payload);
    static uint16_t readTlvU16(const FrameResponse& resp, uint16_t type, uint16_t defaultValue = 0);
    static uint32_t readTlvU32(const FrameResponse& resp, uint16_t type, uint32_t defaultValue = 0);

    void drainRxBuffer();
    void parseOneFrame(const QByteArray& frame);
    void parseServiceBody(uint16_t serviceId, const QByteArray& body, uint8_t seq, uint8_t frameType);
    bool tryTakeResponse(uint8_t seq, FrameResponse* out);
    bool tryTakeStatusNotify(uint16_t* errorCode);
    void sendFrameChunked(const QByteArray& frame);
    bool sendOtaRequest(const QByteArray& tlvBlob, uint8_t* outSeq);
    bool waitResponse(uint8_t expectSeq, FrameResponse* out, int timeoutMs, CancelPredicate cancelled);
    bool startUpgrade(uint32_t imageId, uint32_t version, const QByteArray& imageData, uint32_t* outOffset,
                      CancelPredicate cancelled, QString* errorOut);
    bool writeBlock(uint32_t offset, const QByteArray& chunk, CancelPredicate cancelled, QString* errorOut);
    bool endUpgrade(CancelPredicate cancelled, QString* errorOut);

    SendFunc sendFunc_;
    LogFunc logFunc_;
    int transferIntervalMs_ = 0;
    int uartChunkSize_ = kDefaultUartChunkSize;
    QByteArray rxBuffer_;
    QQueue<FrameResponse> responses_;
    QQueue<uint16_t> statusNotifies_;
    uint8_t seq_ = 0;
};

#endif // ROOT_BLE_OTA2_H
