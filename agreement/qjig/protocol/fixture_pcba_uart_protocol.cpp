#include "fixture_pcba_uart_protocol.h"

#include <QCoreApplication>
#include <QDebug>

#include "Abini.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

namespace {

constexpr uint8_t kExtUartPhyLayerMagic = 0x55;
constexpr int kFixPhyLayerHeadSize = 1;
constexpr int kFixPhyLayerCrcSize = 1;
constexpr int kFixPhyLayerHeaderAndCrc = kFixPhyLayerHeadSize + kFixPhyLayerCrcSize;

#pragma pack(push, 1)
struct ext_uart_phy_layer_t {
    uint8_t magic;
    uint8_t mechine;
    uint8_t length;
    uint16_t staticCurrent;
    uint16_t workingCurrent;
    uint8_t overVoltageLight;
    uint8_t button1;
    uint8_t button2;
    uint16_t chargingCurrent;
    uint8_t music_state;
    uint16_t musicCurrent;
    uint16_t shipCurrent;
    uint8_t fixerro;
    uint8_t end;
};
#pragma pack(pop)

constexpr int kFixPhyLayerFrameSize = static_cast<int>(sizeof(ext_uart_phy_layer_t));

constexpr int kPcbaMachineSlotMax = 0x0F;  // 机位 1..15（0x01..0x0F）

QByteArray buildIndexedHexCommand(int machineIndex, const char* const* hexTable, int tableSize) {
    if (machineIndex <= 0 || machineIndex > tableSize) {
        return QByteArray();
    }
    return QByteArray::fromHex(hexTable[machineIndex - 1]);
}

}  // namespace

FixturePcbaUartProtocol::FixturePcbaUartProtocol(RingBuf* ringBuf, usmile_ring_buffer_t* ring, uint8_t* frameBuf,
                                                 int frameBufSize)
    : ringBuf_(ringBuf), ring_(ring), frameBuf_(frameBuf), frameBufSize_(frameBufSize) {}

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
        ringBuf_->usmile_ring_buffer_pick(ring_, frameBuf_, kFixPhyLayerHeadSize + kFixPhyLayerFrameSize);
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

        const int frameSize = head->length;
        if (frameSize > ringSize) {
            break;
        }

        ringBuf_->usmile_ring_buffer_pick(ring_, frameBuf_, frameSize);

        if (frameBuf_[head->length - 1] == 0xAA) {
            const QByteArray buffer(reinterpret_cast<const char*>(head), frameSize);
            if (head->length == 0x05) {
                dispatchShortFrame(buffer, handler);
            } else {
                const FixturePcbaUartEvent ev = parseFullFrame(buffer);
                if (ev.type != FixturePcbaUartEvent::Type::None) {
                    handler(ev);
                }
            }
        } else {
            qDebug() << "head content:" << QByteArray(reinterpret_cast<char*>(head), head->length * 2).toHex();
            qDebug() << "尾巴校验失败" << frameBuf_[head->length - 1] << head->end << head->chargingCurrent
                     << head->magic;
        }

        ringBuf_->usmile_ring_buffer_delete(ring_, frameSize);
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
    QByteArray receivebuf = data;

    if (receivebuf.size() > receivebuf.at(2)) {
        qDebug() << "收到的pcba治具数据大于" << receivebuf.at(2);
        for (int i = 0; i < receivebuf.size(); i++) {
            if (static_cast<int>(receivebuf.at(i)) == 85) {
                const uchar length = static_cast<uchar>(receivebuf.at(2));
                qDebug() << "i====" << i;
                receivebuf = receivebuf.remove(0, i);
                receivebuf = receivebuf.remove(length, receivebuf.size());
                qDebug() << "截取包头到包尾内容";
            }
        }
        qDebug() << "receivebuf.size()====" << receivebuf.size();
        if (receivebuf.size() > receivebuf.at(2)) {
            return ev;
        }
    }

    qDebug() << "开始打包治具数据包";
    FixturePacketData datapack;
    qDebug() << "数值为" << receivebuf.toHex();
    qDebug() << "数值为" << receivebuf.size();
    qDebug() << "数值为" << static_cast<int>(receivebuf.at(0))
             << static_cast<int>(receivebuf.at(receivebuf.size() - 1));

    if (receivebuf.size() < 3) {
        qDebug() << "接收到的数据包长度不足";
        return ev;
    }

    const uchar length = static_cast<uchar>(receivebuf.at(2));
    qDebug() << "length为：" << length;

    if (receivebuf.size() < static_cast<int>(length)) {
        qDebug() << "接收到的数据包长度不足";
        return ev;
    }

    if (receivebuf.size() > static_cast<int>(length)) {
        qDebug() << "实际治具串口的长度大于包里的长度说明";
        if (static_cast<int>(receivebuf.at(length)) != -86) {
            qDebug() << "数据有效";
        } else {
            qDebug() << "治具数据无效";
            return ev;
        }
    }

    if (static_cast<int>(receivebuf.at(0)) != 85 || static_cast<int>(receivebuf.at(receivebuf.size() - 1)) != -86) {
        qDebug() << "接收到的数据包开头或结尾格式不正确";
        return ev;
    }

    datapack.machineNumber = static_cast<uchar>(receivebuf.at(1));
    datapack.staticCurrent =
        (static_cast<uint8_t>(receivebuf.at(3)) << 8) | static_cast<uint8_t>(receivebuf.at(4));
    datapack.workingCurrent =
        (static_cast<uint8_t>(receivebuf.at(5)) << 8) | static_cast<uint8_t>(receivebuf.at(6));
    datapack.overVoltageLight = static_cast<uchar>(receivebuf.at(7));
    datapack.button1 = static_cast<uchar>(receivebuf.at(8));
    datapack.button2 = static_cast<uchar>(receivebuf.at(9));
    datapack.chargingCurrent =
        (static_cast<uint8_t>(receivebuf.at(10)) << 8) | static_cast<uint8_t>(receivebuf.at(11));

    if (SETTINGS.value("SYSTEM/TestAudioCurrent").toBool()) {
        datapack.musicCurrent =
            (static_cast<uint8_t>(receivebuf.at(12)) << 8) | static_cast<uint8_t>(receivebuf.at(13));
    } else {
        datapack.music_state = static_cast<uchar>(receivebuf.at(12));
    }
    if (SETTINGS.value("SYSTEM/TestShippingCurrent").toBool()) {
        datapack.shipCurrent =
            (static_cast<uint8_t>(receivebuf.at(14)) << 8) | static_cast<uint8_t>(receivebuf.at(15));
    }
    if (receivebuf.size() >= 17) {
        datapack.fixerro = static_cast<uint8_t>(receivebuf.at(16));
    }

    qDebug() << "机号:" << datapack.machineNumber;
    qDebug() << "静态电流值:" << datapack.staticCurrent << "ua";
    qDebug() << "工作电流:" << datapack.workingCurrent << "ma";
    qDebug() << "过压灯正常:" << datapack.overVoltageLight;
    qDebug() << "按键1:" << datapack.button1;
    qDebug() << "按键2:" << datapack.button2;
    qDebug() << "充电电流:" << datapack.chargingCurrent << "ma";
    qDebug() << "治具错误码:" << datapack.fixerro;
    qDebug() << "船运电流:" << datapack.shipCurrent;
    if (SETTINGS.value("SYSTEM/TestAudioCurrent").toBool()) {
        qDebug() << "音频电流:" << datapack.musicCurrent;
    } else {
        qDebug() << "音频情况:" << datapack.music_state;
    }

    ev.type = FixturePcbaUartEvent::Type::FullPacket;
    ev.packet = datapack;
    return ev;
}

QByteArray FixturePcbaUartProtocol::buildStartTestCommand(int machineIndex) {
    static const char* kCommands[] = {"AA01", "AA02", "AA03", "AA04", "AA05", "AA06", "AA07", "AA08",
                                      "AA09", "AA0A", "AA0B", "AA0C", "AA0D", "AA0E", "AA0F"};
    return buildIndexedHexCommand(machineIndex, kCommands, kPcbaMachineSlotMax);
}

QByteArray FixturePcbaUartProtocol::buildSleepCommand(int machineIndex) {
    static const char* kCommands[] = {"CC01", "CC02", "CC03", "CC04", "CC05", "CC06", "CC07", "CC08",
                                      "CC09", "CC0A", "CC0B", "CC0C", "CC0D", "CC0E", "CC0F"};
    return buildIndexedHexCommand(machineIndex, kCommands, kPcbaMachineSlotMax);
}

QByteArray FixturePcbaUartProtocol::buildWhiteModeCommand(int machineIndex) {
    static const char* kCommands[] = {"EE01", "EE02", "EE03", "EE04", "EE05", "EE06", "EE07", "EE08",
                                      "EE09", "EE0A", "EE0B", "EE0C", "EE0D", "EE0E", "EE0F"};
    return buildIndexedHexCommand(machineIndex, kCommands, kPcbaMachineSlotMax);
}

#if _MSC_VER >= 1600
#    pragma execution_character_set(pop)
#endif
