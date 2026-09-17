#ifndef JIELI_BT_BOX_DEVICE_H
#define JIELI_BT_BOX_DEVICE_H

#include "jieli_bt_box_codec.h"

#include <functional>
#include <QString>

class SerialChannel;
class QFixtureManager;

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 杰理蓝牙盒子：上电后串口主动上报频偏/RSSI TLV 帧，本设备负责超时内拼包解析。 */
class JieliBtBoxDevice {
  public:
    /**
     * 等待完整频偏+RSSI。expectedSide：-1 任意，0 左耳，1 右耳。
     * expectedMac 为 6 字节时优先按 MAC 收下本窗产品（1拖2 两设备同时上报）。
     */
    static bool waitForRfInfo(SerialChannel* channel, int timeoutMs, JieliBtBoxRfInfo* out,
                              QString* errorMessage = nullptr, int expectedSide = -1,
                              const QByteArray& expectedMac = QByteArray(),
                              const std::function<bool()>& shouldContinue = {});
    static bool waitForRfInfo(QFixtureManager* fixture, int timeoutMs, JieliBtBoxRfInfo* out,
                              QString* errorMessage = nullptr, int expectedSide = -1,
                              const QByteArray& expectedMac = QByteArray(),
                              const std::function<bool()>& shouldContinue = {});
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // JIELI_BT_BOX_DEVICE_H
