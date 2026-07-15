#ifndef JIELI_BT_BOX_DEVICE_H
#define JIELI_BT_BOX_DEVICE_H

#include "jieli_bt_box_codec.h"

#include <QString>

class SerialChannel;

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 杰理蓝牙盒子：上电后串口主动上报频偏/RSSI TLV 帧，本设备负责超时内拼包解析。 */
class JieliBtBoxDevice {
  public:
    static bool waitForRfInfo(SerialChannel* channel, int timeoutMs, JieliBtBoxRfInfo* out,
                              QString* errorMessage = nullptr);
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // JIELI_BT_BOX_DEVICE_H
