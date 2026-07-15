#ifndef IMU_FIXTURE_DEVICE_H
#define IMU_FIXTURE_DEVICE_H

#include <functional>

#include <QByteArray>

#include "fixture_uart_types.h"
#include "imu_uart_codec.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达六轴 IMU 治具：ASCII 命令收发。 */
class ImuFixtureDevice {
  public:
    using WriteFn = std::function<void(const QByteArray& data, bool logTx, bool startAction)>;

    explicit ImuFixtureDevice(WriteFn writeFn);

    void sendState(imuFixtureState state);
    FixtureImuUartEvent parseReceived(const QByteArray& data) const;

  private:
    WriteFn write_;
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // IMU_FIXTURE_DEVICE_H
