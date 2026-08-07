#include "xinjie_plc_rtu_types.h"

void registerXinjePlcCmdMetaTypes() {
    qRegisterMetaType<XinjePlcCoilRequest>();
    qRegisterMetaType<XinjePlcRegisterRequest>();
}
