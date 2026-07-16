#ifndef AGILENT_66319D_PROFILE_H
#define AGILENT_66319D_PROFILE_H

#include "huiling_wfp60h_profile.h"

#include <QVariantMap>

/** Agilent/Keysight 66319B/D 移动通信直流源；SCPI 字段与 HuilingWfp60hScpiProfile 相同。 */
using Agilent66319dScpiProfile = HuilingWfp60hScpiProfile;

namespace Agilent66319dScpiProfileUtil {

Agilent66319dScpiProfile defaults();
/** 从 [VisaPower] 加载；缺省键使用 66319D 手册 SCPI（VOLT/CURR/OUTP/MEAS）。 */
Agilent66319dScpiProfile fromVisaPowerSettings();
Agilent66319dScpiProfile fromParamMap(const QVariantMap& map);

} // namespace Agilent66319dScpiProfileUtil

#endif // AGILENT_66319D_PROFILE_H
