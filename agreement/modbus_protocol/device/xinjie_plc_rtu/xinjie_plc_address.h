#ifndef XINJIE_PLC_ADDRESS_H
#define XINJIE_PLC_ADDRESS_H

#include <QtGlobal>
#include <QString>

enum class XinjePlcArea {
    Unknown,
    M,
    X,
    Y,
    S,
    D,
    T,
    C,
};

/** 信捷寄存器名解析结果；线圈/离散/保持寄存器由 accessCoil 区分 T/C 双映射。 */
struct XinjePlcAddress {
    XinjePlcArea area = XinjePlcArea::Unknown;
    int index = -1;
    quint16 modbusAddress = 0;
    bool isCoil = false;
    bool isDiscreteInput = false;
    bool isHoldingRegister = false;
    bool ok = false;
    QString error;
};

/** 解析 "M100"/"D500"/"X0"/"Y10" 等；accessCoil=true 时 T/C 走线圈区，否则走保持寄存器区。 */
XinjePlcAddress parseXinjePlcAddress(const QString& text, bool accessCoil);

#endif // XINJIE_PLC_ADDRESS_H
