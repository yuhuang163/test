#include "qprotocol.h"
#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif
qProtocol::qProtocol(QObject* parent) : QObject(parent) {
}

void qProtocol::setSkipDonglePhyWrap(bool skip) {
    skipDonglePhyWrap_ = skip;
}

bool qProtocol::skipDonglePhyWrap() const {
    return skipDonglePhyWrap_;
}
#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
