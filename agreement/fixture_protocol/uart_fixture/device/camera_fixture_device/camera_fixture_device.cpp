#include "camera_fixture_device.h"

#include "qdebug.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

CameraFixtureDevice::CameraFixtureDevice(WriteFn writeFn) : write_(std::move(writeFn)) {
}

void CameraFixtureDevice::sendAction(camreaFixtureState state) {
    const QByteArray frame = FixtureCameraUartProtocol::buildCommand(state);
    write_(frame, true, true);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
