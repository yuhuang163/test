#include "fixture_pcba_uart_protocol.h"

#include <QCoreApplication>
#include <QDebug>

#include "Abini.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr uint8_t kExtUartPhyLayerMagic = 0x55;
constexpr int kFixPhyLayerHeadSize = 1;
constexpr int kFixPhyLayerCrcSize = 1;
constexpr int kFixPhyLayerHeaderAndCrc = kFixPhyLayerHeadSize + kFixPhyLayerCrcSize;

/** PCBA 治具长包：新旧字段合并；线上新包 length=0x17，旧包更短。uint16 为大端，解析用 readBe16 */
#pragma pack(push, 1)
struct ext_uart_phy_layer_t {
    uint8_t magic;
    uint8_t machine;
    uint8_t length;
    uint16_t staticCurrent;  // 静态电流 uA
    uint16_t workingCurrent; // 工作电流 mA
    uint8_t overVoltageLight;
    uint8_t button1;
    uint8_t button2;
    uint16_t chargingCurrent;  // 充电电流 mA
    uint8_t music_state;       // 旧版字节 12：音频状态
    uint16_t musicCurrent;     // 旧:13~14 音频电流；新:12~13 音频 IC mA（见 parse）
    uint16_t standbyCurrentUa; // 字节 14~15：待机电流 uA
    uint16_t pumpVoltageMv;    // 新:16~17 泵电压 mV
    uint16_t mcuVoltageMv;     // 新:18~19 MCU 电压 mV
    uint16_t batteryVoltageMv; // 新:20~21 电池电压 mV
    uint8_t fixerro;           // 旧版字节 17：治具错误码
    uint8_t trailer;           // 包尾 0xAA（新:22，旧:18）
};
#pragma pack(pop)

constexpr int kPcbaShortFrameLen = 0x05;
constexpr int kPcbaFullFrameLenV2 = 0x17;
/** 含 fixerro 字节 + 0xAA 尾，整帧 length=0x18 */
constexpr int kPcbaFullFrameLenWithFixerro = 0x18;
constexpr int kFixPhyLayerFramePeekSize = kPcbaFullFrameLenWithFixerro;

uint16_t readBe16(const QByteArray& buf, int hiIndex) {
    return static_cast<uint16_t>((static_cast<uint8_t>(buf.at(hiIndex)) << 8) | static_cast<uint8_t>(buf.at(hiIndex + 1)));
}

void parseFixturePacketCommonHead(const QByteArray& receivebuf, FixturePacketData& datapack) {
    datapack.machineNumber = static_cast<uchar>(receivebuf.at(1));
    datapack.staticCurrent = readBe16(receivebuf, 3);
    datapack.workingCurrent = readBe16(receivebuf, 5);
    datapack.overVoltageLight = static_cast<uchar>(receivebuf.at(7));
    datapack.button1 = static_cast<uchar>(receivebuf.at(8));
    datapack.button2 = static_cast<uchar>(receivebuf.at(9));
    datapack.chargingCurrent = readBe16(receivebuf, 10);
}

/** 包头 length 须与 buffer 字节数一致，否则返回 false */
bool parseFixturePacket(const QByteArray& receivebuf, FixturePacketData& datapack) {
    if (receivebuf.size() < 3)
        return false;
    const int declaredLen = static_cast<uchar>(receivebuf.at(2));
    if (receivebuf.size() != declaredLen) {
        qDebug() << QStringLiteral("治具包长度不符：包头length=%1 实收=%2")
                        .arg(declaredLen)
                        .arg(receivebuf.size());
        return false;
    }
    if (declaredLen < 12) {
        qDebug() << QStringLiteral("治具包 length 过短：%1").arg(declaredLen);
        return false;
    }
    if (static_cast<uchar>(receivebuf.at(0)) != 0x55 || static_cast<uchar>(receivebuf.at(declaredLen - 1)) != 0xAA) {
        qDebug() << QStringLiteral("治具包头尾格式错误 length=%1").arg(declaredLen);
        return false;
    }

    parseFixturePacketCommonHead(receivebuf, datapack);
    datapack.music_state = 0;
    datapack.musicCurrent = 0;
    datapack.standbyCurrentUa = 0;
    datapack.pumpVoltageMv = 0;
    datapack.mcuVoltageMv = 0;
    datapack.batteryVoltageMv = 0;
    datapack.fixerro = 0;

    if (declaredLen >= kPcbaFullFrameLenV2) {
        datapack.musicCurrent = readBe16(receivebuf, 12);
        datapack.standbyCurrentUa = readBe16(receivebuf, 14);
        datapack.pumpVoltageMv = readBe16(receivebuf, 16);
        datapack.mcuVoltageMv = readBe16(receivebuf, 18);
        datapack.batteryVoltageMv = readBe16(receivebuf, 20);
        if (declaredLen >= kPcbaFullFrameLenWithFixerro)
            datapack.fixerro = static_cast<uint8_t>(receivebuf.at(22));
        return true;
    }

    if (SETTINGS.value("SYSTEM/TestAudioCurrent").toBool()) {
        if (declaredLen >= 14)
            datapack.musicCurrent = readBe16(receivebuf, 12);
    } else if (declaredLen > 12) {
        datapack.music_state = static_cast<uchar>(receivebuf.at(12));
    }
    if (SETTINGS.value("SYSTEM/TestShippingCurrent").toBool() && declaredLen > 14)
        datapack.standbyCurrentUa = readBe16(receivebuf, 14);
    if (declaredLen >= 18)
        datapack.fixerro = static_cast<uint8_t>(receivebuf.at(17));
    return true;
}

constexpr int kPcbaMachineSlotMax = 0x0F; // 机位 1..15（0x01..0x0F）

QByteArray buildPcbaMachineCommand(uint8_t opcode, int machineIndex) {
    if (machineIndex < 1 || machineIndex > kPcbaMachineSlotMax)
        return QByteArray();
    QByteArray frame;
    frame.reserve(2);
    frame.append(static_cast<char>(opcode));
    frame.append(static_cast<char>(machineIndex & 0xFF));
    return frame;
}

} // namespace

FixturePcbaUartProtocol::FixturePcbaUartProtocol(RingBuf* ringBuf, usmile_ring_buffer_t* ring, uint8_t* frameBuf,
                                                 int frameBufSize)
    : ringBuf_(ringBuf), ring_(ring), frameBuf_(frameBuf), frameBufSize_(frameBufSize) {
}

int FixturePcbaUartProtocol::findNextFrame() {
    int i = 0;
    ext_uart_phy_layer_t* head = nullptr;
    const int len = ringBuf_->usmile_ring_buffer_pick(ring_, frameBuf_, frameBufSize_);

    for (i = 0; i < len; i++) {
        if (frameBuf_[i] == 0x55) {
            head = reinterpret_cast<ext_uart_phy_layer_t*>(&frameBuf_[i]);
            if (head->magic == kExtUartPhyLayerMagic) {
                qDebug() << "匹配到了串口数据包头";
                ringBuf_->usmile_ring_buffer_delete(ring_, i);
                return 1;
            }
        }
    }

    ringBuf_->usmile_ring_buffer_delete(ring_, len);
    return 0;
}

void FixturePcbaUartProtocol::pollFrames(const std::function<void(const FixturePcbaUartEvent&)>& handler) {
    while (true) {
        ringBuf_->usmile_ring_buffer_pick(ring_, frameBuf_, kFixPhyLayerHeadSize + kFixPhyLayerFramePeekSize);
        const int ringSize = ringBuf_->usmile_ring_buffer_items_count_get(ring_);
        if (ringSize <= kFixPhyLayerHeaderAndCrc) {
            break;
        }

        auto* head = reinterpret_cast<ext_uart_phy_layer_t*>(frameBuf_);
        if (head->magic != kExtUartPhyLayerMagic) {
            qDebug() << "串口数据流错误寻找下一帧";
            qDebug() << "串口数据包头为:"
                     << QByteArray(reinterpret_cast<char*>(frameBuf_), kFixPhyLayerHeadSize).toHex();
            if (findNextFrame()) {
                continue;
            }
            break;
        }

        const int declaredLen = head->length;
        if (declaredLen <= 0) {
            ringBuf_->usmile_ring_buffer_delete(ring_, 1);
            continue;
        }
        if (declaredLen > ringSize) {
            break;
        }

        ringBuf_->usmile_ring_buffer_pick(ring_, frameBuf_, declaredLen);

        if (frameBuf_[declaredLen - 1] != 0xAA) {
            const QString frameHex =
                QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(head), declaredLen).toHex());
            qDebug() << QStringLiteral("治具包尾校验失败 length=%1 尾字节=0x%2 帧=%3")
                            .arg(declaredLen)
                            .arg(static_cast<int>(frameBuf_[declaredLen - 1]), 2, 16, QLatin1Char('0'))
                            .arg(frameHex);
            ringBuf_->usmile_ring_buffer_delete(ring_, declaredLen);
            continue;
        }

        const QByteArray buffer(reinterpret_cast<const char*>(head), declaredLen);
        if (declaredLen == kPcbaShortFrameLen) {
            dispatchShortFrame(buffer, handler);
        } else {
            const FixturePcbaUartEvent ev = parseFullFrame(buffer);
            if (ev.type != FixturePcbaUartEvent::Type::None) {
                handler(ev);
            }
        }

        ringBuf_->usmile_ring_buffer_delete(ring_, declaredLen);
        QCoreApplication::processEvents();
    }
}

void FixturePcbaUartProtocol::dispatchShortFrame(const QByteArray& data,
                                                 const std::function<void(const FixturePcbaUartEvent&)>& handler) {
    if (data.size() == 5 && static_cast<uchar>(data.at(3)) == 0xCC) {
        FixturePcbaUartEvent ev;
        ev.type = FixturePcbaUartEvent::Type::ShortSleep;
        ev.packet.machineNumber = static_cast<uchar>(data.at(1));
        ev.packet.sleep = 1;
        qDebug() << "开始休眠";
        handler(ev);
    }

    qDebug() << "data.at(0)" << data.at(0) << data.size();
    if (data.size() == 5 && static_cast<uchar>(data.at(1)) == 0xAA && static_cast<uchar>(data.at(3)) == 0xAA) {
        qDebug() << "开始测试";
        FixturePcbaUartEvent ev;
        ev.type = FixturePcbaUartEvent::Type::ShortStart;
        handler(ev);
    }
}

FixturePcbaUartEvent FixturePcbaUartProtocol::parseFullFrame(const QByteArray& data) {
    FixturePcbaUartEvent ev;
    const QByteArray& receivebuf = data;

    qDebug() << "开始打包治具数据包";
    qDebug() << "数值为" << receivebuf.toHex();
    qDebug() << "数值为" << receivebuf.size();

    if (receivebuf.size() < 3) {
        qDebug() << "接收到的数据包长度不足";
        return ev;
    }

    const uchar declaredLen = static_cast<uchar>(receivebuf.at(2));
    qDebug() << "length为：" << declaredLen << "wireLen=" << receivebuf.size();

    FixturePacketData datapack;
    if (!parseFixturePacket(receivebuf, datapack)) {
        return ev;
    }

    const bool extended = declaredLen >= kPcbaFullFrameLenV2;
    qDebug() << "机号:" << datapack.machineNumber
             << QStringLiteral("(length=0x%1)").arg(declaredLen, 2, 16, QLatin1Char('0'));
    qDebug() << "静态电流值:" << datapack.staticCurrent << "uA";
    qDebug() << "工作电流:" << datapack.workingCurrent << "mA";
    qDebug() << "过压灯正常:" << datapack.overVoltageLight;
    qDebug() << "按键1:" << datapack.button1;
    qDebug() << "按键2:" << datapack.button2;
    qDebug() << "充电电流:" << datapack.chargingCurrent << "mA";
    if (extended) {
        qDebug() << "音频IC电流:" << datapack.musicCurrent << "mA";
        qDebug() << "待机电流:" << datapack.standbyCurrentUa << "uA";
        qDebug() << "泵电压:" << datapack.pumpVoltageMv << "mV";
        qDebug() << "MCU电压:" << datapack.mcuVoltageMv << "mV";
        qDebug() << "电池电压:" << datapack.batteryVoltageMv << "mV";
        qDebug() << "治具错误码:" << datapack.fixerro;
    } else {
        qDebug() << "治具错误码:" << datapack.fixerro;
        qDebug() << "待机电流:" << datapack.standbyCurrentUa << "uA";
        if (SETTINGS.value("SYSTEM/TestAudioCurrent").toBool())
            qDebug() << "音频电流:" << datapack.musicCurrent << "mA";
        else
            qDebug() << "音频情况:" << datapack.music_state;
    }

    ev.type = FixturePcbaUartEvent::Type::FullPacket;
    ev.packet = datapack;
    return ev;
}

QByteArray FixturePcbaUartProtocol::buildStartTestCommand(int machineIndex) {
    return buildPcbaMachineCommand(0xAA, machineIndex);
}

QByteArray FixturePcbaUartProtocol::buildSleepCommand(int machineIndex) {
    return buildPcbaMachineCommand(0xCC, machineIndex);
}

QByteArray FixturePcbaUartProtocol::buildWhiteModeCommand(int machineIndex) {
    return buildPcbaMachineCommand(0xEE, machineIndex);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
