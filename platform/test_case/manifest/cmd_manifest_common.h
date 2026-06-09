#ifndef PLATFORM_CMD_MANIFEST_COMMON_H
#define PLATFORM_CMD_MANIFEST_COMMON_H

#include "test_case_types.h"

#include <cstdint>

/** 各通道指令清单（Device/Dongle/Fixture 等）共用的 Set/Get 标志。 */
namespace TestCaseCmdManifest {

constexpr uint8_t kSendActionSet = 0x01;
constexpr uint8_t kSendActionGet = 0x02;
constexpr uint8_t kSendActionBoth = kSendActionSet | kSendActionGet;

inline bool matchesSendAction(uint8_t rowFlags, TestCaseSendAction action) {
    if ((rowFlags & kSendActionBoth) == kSendActionBoth)
        return action == TestCaseSendAction::Set || action == TestCaseSendAction::Get;
    if (rowFlags & kSendActionGet)
        return action == TestCaseSendAction::Get;
    return action == TestCaseSendAction::Set;
}

inline TestCaseSendAction defaultSendAction(uint8_t rowFlags) {
    if ((rowFlags & kSendActionBoth) == kSendActionBoth)
        return TestCaseSendAction::Set;
    if (rowFlags & kSendActionGet)
        return TestCaseSendAction::Get;
    return TestCaseSendAction::Set;
}

} // namespace TestCaseCmdManifest

#endif // PLATFORM_CMD_MANIFEST_COMMON_H
