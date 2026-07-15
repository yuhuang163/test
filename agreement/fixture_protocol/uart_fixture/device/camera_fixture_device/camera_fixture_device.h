#ifndef CAMERA_FIXTURE_DEVICE_H
#define CAMERA_FIXTURE_DEVICE_H

#include <functional>

#include <QByteArray>

#include "camera_uart_codec.h"
#include "fixture_uart_types.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 摄像工站通道治具：CTL_INx / CTL_OUTx。 */
class CameraFixtureDevice {
  public:
    using WriteFn = std::function<void(const QByteArray& data, bool logTx, bool startAction)>;

    explicit CameraFixtureDevice(WriteFn writeFn);

    void sendAction(camreaFixtureState state);

  private:
    WriteFn write_;
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // CAMERA_FIXTURE_DEVICE_H
