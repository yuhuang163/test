#ifndef XWD_FIXTURE_TYPES_H
#define XWD_FIXTURE_TYPES_H

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

/** 欣旺达 xwd 治具命令（气缸/继电器/摆幅等步骤语义）。 */
enum class XwdFixtureCmd {
    SendState,
    SetCylinderState,
    SetRelayState,
    GetAmplitude,
};

/** 欣旺达 xwd 气缸/继电器状态字。 */
enum XwdFixtureState {
    XWD_STATE_CYLINDER_OPEN,
    XWD_STATE_RELAY_OPEN,
    XWD_STATE_RELAY_RESET,
    XWD_STATE_RELAY1_OPEN,
    XWD_STATE_RELAY1_RESET,
    XWD_STATE_RELAY2_OPEN,
    XWD_STATE_RELAY2_RESET,
    XWD_STATE_CYLINDER_RESET,
    XWD_STATE_RELAY4_OPEN,
    XWD_STATE_RELAY4_RESET,
    XWD_STATE_RESET_ALL,
    XWD_STATE_PEN_PRESS,
    XWD_STATE_TRAY_MIDDLE,
    XWD_STATE_TRAY_REAR,
    XWD_STATE_FRONT,
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // XWD_FIXTURE_TYPES_H
