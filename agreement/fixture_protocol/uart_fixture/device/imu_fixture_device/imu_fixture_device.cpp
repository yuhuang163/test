#include "imu_fixture_device.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

ImuFixtureDevice::ImuFixtureDevice(WriteFn writeFn) : write_(std::move(writeFn)) {
}

void ImuFixtureDevice::sendState(imuFixtureState state) {
    const QByteArray frame = FixtureImuUartProtocol::buildCommand(state);
    write_(frame, true, true);
}

FixtureImuUartEvent ImuFixtureDevice::parseReceived(const QByteArray& data) const {
    return FixtureImuUartProtocol::parseReceived(data);
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
