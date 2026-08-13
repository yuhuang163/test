#include "xinjie_plc_address.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

namespace {

int parseIndexNumber(const QString& numText, XinjePlcArea area) {
    const QString trimmed = numText.trimmed();
    if (trimmed.isEmpty()) {
        return -1;
    }
    bool ok = false;
    if (area == XinjePlcArea::X || area == XinjePlcArea::Y) {
        const int oct = trimmed.toInt(&ok, 8);
        return ok ? oct : -1;
    }
    const int dec = trimmed.toInt(&ok, 10);
    return ok ? dec : -1;
}

} // namespace

XinjePlcAddress parseXinjePlcAddress(const QString& text, bool accessCoil) {
    XinjePlcAddress out;
    const QString raw = text.trimmed();
    if (raw.size() < 2) {
        out.error = QStringLiteral("信捷地址过短: %1").arg(raw);
        return out;
    }

    const QChar prefix = raw.at(0).toUpper();
    switch (prefix.unicode()) {
    case 'M':
        out.area = XinjePlcArea::M;
        break;
    case 'X':
        out.area = XinjePlcArea::X;
        break;
    case 'Y':
        out.area = XinjePlcArea::Y;
        break;
    case 'S':
        out.area = XinjePlcArea::S;
        break;
    case 'D':
        out.area = XinjePlcArea::D;
        break;
    case 'T':
        out.area = XinjePlcArea::T;
        break;
    case 'C':
        out.area = XinjePlcArea::C;
        break;
    default:
        out.error = QStringLiteral("不支持的信捷地址前缀: %1").arg(raw);
        return out;
    }

    out.index = parseIndexNumber(raw.mid(1), out.area);
    if (out.index < 0) {
        out.error = QStringLiteral("信捷地址编号非法: %1").arg(raw);
        return out;
    }

    switch (out.area) {
    case XinjePlcArea::M:
        if (out.index > 7999) {
            out.error = QStringLiteral("M 地址超出 0-7999: %1").arg(raw);
            return out;
        }
        out.modbusAddress = quint16(out.index);
        out.isCoil = true;
        out.ok = true;
        break;
    case XinjePlcArea::D:
        if (out.index > 7999) {
            out.error = QStringLiteral("D 地址超出 0-7999: %1").arg(raw);
            return out;
        }
        out.modbusAddress = quint16(out.index);
        out.isHoldingRegister = true;
        out.ok = true;
        break;
    case XinjePlcArea::X:
        out.modbusAddress = quint16(0x4000 + out.index);
        out.isDiscreteInput = true;
        out.ok = true;
        break;
    case XinjePlcArea::Y:
        out.modbusAddress = quint16(0x4800 + out.index);
        out.isCoil = true;
        out.ok = true;
        break;
    case XinjePlcArea::S:
        if (out.index > 1023) {
            out.error = QStringLiteral("S 地址超出 0-1023: %1").arg(raw);
            return out;
        }
        out.modbusAddress = quint16(0x5000 + out.index);
        out.isCoil = true;
        out.ok = true;
        break;
    case XinjePlcArea::T:
        if (out.index > 618) {
            out.error = QStringLiteral("T 地址超出 0-618: %1").arg(raw);
            return out;
        }
        if (accessCoil) {
            out.modbusAddress = quint16(0x6400 + out.index);
            out.isCoil = true;
        } else {
            out.modbusAddress = quint16(0x3000 + out.index);
            out.isHoldingRegister = true;
        }
        out.ok = true;
        break;
    case XinjePlcArea::C:
        if (out.index > 634) {
            out.error = QStringLiteral("C 地址超出 0-634: %1").arg(raw);
            return out;
        }
        if (accessCoil) {
            out.modbusAddress = quint16(0x6C00 + out.index);
            out.isCoil = true;
        } else {
            out.modbusAddress = quint16(0x3800 + out.index);
            out.isHoldingRegister = true;
        }
        out.ok = true;
        break;
    default:
        out.error = QStringLiteral("未知信捷地址: %1").arg(raw);
        break;
    }
    return out;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
