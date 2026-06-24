#ifndef SCPI_TYPES_H
#define SCPI_TYPES_H

#include <QString>

/**
 * SCPI 域 access：链路类型、设备路由、统一 Cmd。
 * 物理驱动在 platform/driver/serial、platform/driver/visa；本域 manager 组 SCPI 行。
 */

/** 物理链路：串口与 VISA 仅底层驱动不同，上层均为 SCPI 文本行。 */
enum class ScpiLinkKind {
    Serial,
    Visa,
};

/** SCPI 设备路由（SETTINGS / 工站配置，非 device 目录名）。 */
enum class ScpiDeviceRoute {
    None,
    HuilingWfp60h,
    RsCmw100,
};

struct ScpiVisaLinkConfig {
    QString visaAddress;
    int timeoutMs = 3000;
};

enum class UsbProtocolRoute {
    Auto,
    Scpi,
    HqModbus,
    LxModbus,
};

struct UsbLinkConfig {
    UsbProtocolRoute protocol = UsbProtocolRoute::Auto;
    int luxshareMachineId = 1;
};

#endif // SCPI_TYPES_H
