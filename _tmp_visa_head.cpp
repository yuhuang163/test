#include "visa_channel.h"

#include <QCoreApplication>
#include <QDebug>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QThread>
#include <QDateTime>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

#ifdef HAVE_NI_VISA
namespace {

// ensureConnected / write / read / close ????????
QRecursiveMutex& visaGlobalMutex() {
    static QRecursiveMutex mutex;
    return mutex;
}

ViSession& sharedResourceManager() {
    static ViSession rm = VI_NULL;
    return rm;
}

struct SharedInstrument {
    ViSession session = VI_NULL;
    int refCount = 0;
    int timeoutMs = 3000;
    qint64 openedAtMs = 0;
    qint64 lastIoAtMs = 0;
};

QHash<QString, SharedInstrument>& sharedInstruments() {
    static QHash<QString, SharedInstrument> map;
    return map;
}

QHash<QString, qint64>& lastGpiBCloseMs() {
    static QHash<QString, qint64> map;
    return map;
}

QString visaStatusText(ViStatus status) {
    ViChar desc[256] = {0};
    QString text;
    if (viStatusDesc(VI_NULL, status, desc) >= VI_SUCCESS && desc[0] != '\0')
        text = QString::fromLocal8Bit(desc);
    else
        text = QStringLiteral("status=%1").arg(static_cast<int>(status));
    // ????????????????? visa.h ??VI_ERROR_*???????????NI ?????DLL??
    return QStringLiteral("%1 (0x%2/%3)")
        .arg(text)
        .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'))
        .arg(static_cast<int>(status));
}

bool isGpiBResource(const QString& address) {
    return address.startsWith(QStringLiteral("GPIB"), Qt::CaseInsensitive);
}

bool isAsrlResource(const QString& address) {
    return address.startsWith(QStringLiteral("ASRL"), Qt::CaseInsensitive);
}

/** ??TCPIP ????????????GPIB ?????????????? ABORT?????viClose??SRL ???????*/
bool shouldKeepIdleSession(const QString& address) {
    return address.startsWith(QStringLiteral("TCPIP"), Qt::CaseInsensitive);
}

/** ????????? msleep?????processEvents???????????AT??? GPIB ?????ABORT ???????
 *  ?????umpDelayMs??????????????Socket??*/
void busSettleDelayMs(int ms, QRecursiveMutex* releaseMutex = nullptr) {
    if (ms <= 0)
        return;
    if (releaseMutex) {
        releaseMutex->unlock();
        QThread::msleep(static_cast<unsigned long>(ms));
        releaseMutex->lock();
        return;
    }
    QElapsedTimer timer;
    timer.start();
    constexpr QEventLoop::ProcessEventsFlags kPumpFlags = QEventLoop::ExcludeSocketNotifiers;
    while (timer.elapsed() < ms) {
        QCoreApplication::processEvents(kPumpFlags, 16);
        const int remain = ms - static_cast<int>(timer.elapsed());
        if (remain <= 0)
            break;
        QThread::msleep(static_cast<unsigned long>(qMin(16, remain)));
    }
}

void waitGpiBReopenCooldown(const QString& address) {
    const qint64 closedAt = lastGpiBCloseMs().value(address, 0);
    if (closedAt <= 0)
        return;
    const qint64 elapsed = QDateTime::currentMSecsSinceEpoch() - closedAt;
    // ???????????????????????
    constexpr int kMinReopenMs = 400;
    if (elapsed < kMinReopenMs)
        busSettleDelayMs(static_cast<int>(kMinReopenMs - elapsed), &visaGlobalMutex());
}

void trySoftViClear(ViSession inst, const char* context) {
    const ViStatus clearStatus = viClear(inst);
    if (clearStatus < VI_SUCCESS)
        qDebug().noquote() << "VisaChannel:" << context << "viClear ???" << visaStatusText(clearStatus);
}

void flushSessionBuffers(ViSession inst) {
    viFlush(inst, VI_WRITE_BUF);
    viFlush(inst, VI_READ_BUF);
}

/** ASRL??ISA ???????????????/??? RX ???????????????????????PIB ??????????*/
void clearAsrlIoResidue(ViSession inst, const char* context) {
    // ??? VISA ???????????????????????device clear
    viFlush(inst, static_cast<ViUInt16>(VI_READ_BUF_DISCARD | VI_WRITE_BUF_DISCARD | VI_IO_IN_BUF_DISCARD));
    trySoftViClear(inst, context);
}

bool viWriteOnce(ViSession session, const QByteArray& data, ViUInt32* writeCountOut, ViStatus* statusOut) {
    ViUInt32 writeCount = 0;
    const ViStatus status = viWrite(session, reinterpret_cast<ViBuf>(const_cast<char*>(data.constData())),
                                    static_cast<ViUInt32>(data.size()), &writeCount);
    if (writeCountOut)
        *writeCountOut = writeCount;
    if (statusOut)
        *statusOut = status;
    return status >= VI_SUCCESS;
}

/** ??????????*IDN? ???????????? true??*/
bool tryGpiBIdnOnce(ViSession inst, int round) {
    const QByteArray idnCmd = QByteArrayLiteral("*IDN?\n");
    ViUInt32 writeCount = 0;
    ViStatus writeSt = VI_SUCCESS;
    if (!viWriteOnce(inst, idnCmd, &writeCount, &writeSt)) {
        qDebug().noquote() << "VisaChannel: GPIB ??? *IDN? ?????round=" << round
                           << "writeCnt=" << writeCount << visaStatusText(writeSt);
        if (writeSt == VI_ERROR_NLISTENERS || writeSt == VI_ERROR_ABORT) {
            qDebug() << "VisaChannel: ??? ???????? NI-488.2 ?????/ NI MAX ???????????????"
                        "???????? *IDN? ???????????????????GPIB??;
        }
        return false;
    }
    char buf[256] = {0};
    ViUInt32 readCount = 0;
    const ViStatus readSt =
        viRead(inst, reinterpret_cast<ViBuf>(buf), static_cast<ViUInt32>(sizeof(buf) - 1), &readCount);
    if (readSt < VI_SUCCESS || readCount == 0) {
        qDebug().noquote() << "VisaChannel: GPIB ??? *IDN? ?????round=" << round
                           << "writeCnt=" << writeCount << "readCnt=" << readCount << visaStatusText(readSt);
        return false;
    }
    qDebug().noquote() << "VisaChannel: GPIB ??? *IDN? round=" << round << "writeCnt=" << writeCount
                       << visaStatusText(writeSt) << "readCnt=" << readCount << visaStatusText(readSt)
                       << "rsp=" << QString::fromLatin1(buf, static_cast<int>(readCount)).trimmed();
    return true;
}

/**
 * GPIB ???????????? NI-488.2 ???????????*IDN???
 * ?????REN_ASSERT??????????????????? REN_ASSERT_ADDRESS ??? NLISTENERS??
 */
bool warmUpGpiBAfterOpen(ViSession inst) {
    // ?????????????????????? *IDN? ?????ABORT??xbfff0030??
    busSettleDelayMs(500, &visaGlobalMutex());
    if (tryGpiBIdnOnce(inst, 1)) {
        // *IDN? ????????????????????????? VOLT/CURR ??ABORT
        busSettleDelayMs(250, &visaGlobalMutex());
        return true;
    }
    busSettleDelayMs(400, &visaGlobalMutex());

    const ViStatus renSt = viGpibControlREN(inst, VI_GPIB_REN_ASSERT);
    if (renSt < VI_SUCCESS) {
        qDebug().noquote() << "VisaChannel: GPIB REN ??????" << visaStatusText(renSt);
        if (renSt == VI_ERROR_NLISTENERS) {
            qDebug() << "VisaChannel: NLISTENERS ????????????????????????????NI-488.2 ?????????";
        }
    } else {
        qDebug() << "VisaChannel: GPIB REN ?????(???/RMT)";
    }
    busSettleDelayMs(400, &visaGlobalMutex());
    if (tryGpiBIdnOnce(inst, 2)) {
        busSettleDelayMs(250, &visaGlobalMutex());
        return true;
    }
    return false;
}

void configureSessionAfterOpen(ViSession inst, const QString& address, int timeoutMs, int asrlBaudRate) {
    // GPIB?????????????????viWrite/viRead ???????????? TMO
    const int tmo = isGpiBResource(address) ? qBound(1000, timeoutMs, 2000) : qMax(timeoutMs, 5000);
    viSetAttribute(inst, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(tmo));
    viSetAttribute(inst, VI_ATTR_SEND_END_EN, VI_TRUE);
    if (isGpiBResource(address))
        return;
    viSetAttribute(inst, VI_ATTR_DMA_ALLOW_EN, VI_FALSE);
    viSetAttribute(inst, VI_ATTR_TERMCHAR, static_cast<ViAttrState>('\n'));
    viSetAttribute(inst, VI_ATTR_TERMCHAR_EN, VI_TRUE);
    if (isAsrlResource(address)) {
        const int baud = asrlBaudRate > 0 ? asrlBaudRate : 9600;
        viSetAttribute(inst, VI_ATTR_ASRL_BAUD, static_cast<ViAttrState>(baud));
        viSetAttribute(inst, VI_ATTR_ASRL_DATA_BITS, static_cast<ViAttrState>(8));
        viSetAttribute(inst, VI_ATTR_ASRL_PARITY, static_cast<ViAttrState>(VI_ASRL_PAR_NONE));
        viSetAttribute(inst, VI_ATTR_ASRL_STOP_BITS, static_cast<ViAttrState>(VI_ASRL_STOP_ONE));
        viSetAttribute(inst, VI_ATTR_ASRL_FLOW_CNTRL, static_cast<ViAttrState>(VI_ASRL_FLOW_NONE));
        qDebug() << "VisaChannel: ASRL ??????" << address << "baud=" << baud << "8N1";
        clearAsrlIoResidue(inst, "ASRL?????);
        busSettleDelayMs(50);
    } else {
        trySoftViClear(inst, "?????);
    }
}

/** GPIB ????????? *IDN? ??????????????????????????800ms??*/
void settleGpiBBeforeIoLocked(SharedInstrument& slot) {
    if (slot.lastIoAtMs > 0) {
        const qint64 sinceIo = QDateTime::currentMSecsSinceEpoch() - slot.lastIoAtMs;
        // 66319D + USB-GPIB??150ms ??????????????ABORT??????VOLT ??CURR ???
        constexpr qint64 kMinGapMs = 200;
        if (sinceIo < kMinGapMs)
            busSettleDelayMs(static_cast<int>(kMinGapMs - sinceIo), &visaGlobalMutex());
        return;
    }
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (slot.openedAtMs > 0) {
        const qint64 sinceOpen = now - slot.openedAtMs;
        constexpr qint64 kMinAfterOpenMs = 250;
        if (sinceOpen < kMinAfterOpenMs)
            busSettleDelayMs(static_cast<int>(kMinAfterOpenMs - sinceOpen), &visaGlobalMutex());
    }
}

bool viWriteGpiBLocked(ViSession session, const QByteArray& data, ViUInt32* writeCountOut, ViStatus* statusOut) {
    // ??? viLock?????USB-GPIB ??Lock/Unlock ???????? ABORT
    return viWriteOnce(session, data, writeCountOut, statusOut);
}

bool viReadGpiBLocked(ViSession session, ViBuf buf, ViUInt32 count, ViUInt32* readCountOut, ViStatus* statusOut) {
    ViUInt32 readCount = 0;
    const ViStatus status = viRead(session, buf, count, &readCount);
    if (readCountOut)
        *readCountOut = readCount;
    if (statusOut)
        *statusOut = status;
    return status >= VI_SUCCESS;
}

/** ???????????visaGlobalMutex??????????????? GPIB ????????*/
void invalidateSharedSessionLocked(const QString& address, QHash<QString, SharedInstrument>::iterator it,
                                   bool* holdsSharedInstFlag) {
    if (it == sharedInstruments().end()) {
        if (holdsSharedInstFlag)
            *holdsSharedInstFlag = false;
        return;
    }
    if (it->session != VI_NULL) {
        // GPIB ??ABORT ?????flush/clear?????????
        if (!isGpiBResource(address))
            flushSessionBuffers(it->session);
        else
            busSettleDelayMs(50, &visaGlobalMutex());
        viClose(it->session);
        it->session = VI_NULL;
        if (isGpiBResource(address))
            lastGpiBCloseMs().insert(address, QDateTime::currentMSecsSinceEpoch());
        qDebug() << "VisaChannel: ??????????? << address;
    }
    sharedInstruments().erase(it);
    if (holdsSharedInstFlag)
        *holdsSharedInstFlag = false;
}

bool ensureSharedRmLocked(QString* errOut) {
    ViSession& rm = sharedResourceManager();
    if (rm != VI_NULL)
        return true;
    const ViStatus rmStatus = viOpenDefaultRM(&rm);
    if (rmStatus < VI_SUCCESS) {
        rm = VI_NULL;
        if (errOut)
            *errOut = QStringLiteral("??? VISA RM ??? status=%1").arg(static_cast<int>(rmStatus));
        return false;
    }
    return true;
}

void logSharedSessionsLocked(const QString& tag) {
    const Qt::HANDLE tid = QThread::currentThreadId();
    if (sharedInstruments().isEmpty()) {
        qDebug().noquote() << "VisaChannel[" << tag << "] thread=" << tid << " ????????;
        return;
    }
    for (auto it = sharedInstruments().constBegin(); it != sharedInstruments().constEnd(); ++it) {
        qDebug().noquote() << "VisaChannel[" << tag << "] thread=" << tid << "addr=" << it.key()
                           << "ref=" << it.value().refCount
                           << "session=" << (it.value().session != VI_NULL ? "open" : "null")
                           << "lastIoMs=" << it.value().lastIoAtMs;
    }
}

} // namespace
#endif

void VisaChannel::dumpSharedSessions(const QString& tag) {
#ifdef HAVE_NI_VISA
    QMutexLocker locker(&visaGlobalMutex());
    logSharedSessionsLocked(tag);
#else
    Q_UNUSED(tag);
#endif
}

void VisaChannel::pumpDelayMs(int ms) {
#ifdef HAVE_NI_VISA
    busSettleDelayMs(ms, nullptr);
#else
    if (ms > 0)
        QThread::msleep(static_cast<unsigned long>(ms));
#endif
}

void VisaChannel::idleDelayMs(int ms) {
    if (ms > 0)
        QThread::msleep(static_cast<unsigned long>(ms));
}

VisaChannel::VisaChannel(QObject* parent) : QObject(parent) {
}

VisaChannel::~VisaChannel() {
    close();
}

void VisaChannel::setConfig(const Config& config) {
    if (config_.resourceAddress != config.resourceAddress) {
        close();
    }
    config_ = config;
}

VisaChannel::Config VisaChannel::config() const {
    return config_;
}

bool VisaChannel::isOpen() const {
#ifdef HAVE_NI_VISA
    return holdsSharedInst_;
#else
    return false;
#endif
}

#ifdef HAVE_NI_VISA
QString VisaChannel::statusText(ViStatus status) {
    return visaStatusText(status);
}
#endif

bool VisaChannel::ensureConnected() {
#ifdef HAVE_NI_VISA
    QMutexLocker locker(&visaGlobalMutex());
    const QString address = config_.resourceAddress.trimmed();
    if (address.isEmpty()) {
        qDebug() << "VisaChannel: VISA ?????????";
        return false;
    }
    if (holdsSharedInst_) {
        auto it = sharedInstruments().find(address);
        if (it != sharedInstruments().end() && it->session != VI_NULL)
            return true;
        holdsSharedInst_ = false;
    }

    QString rmErr;
    if (!ensureSharedRmLocked(&rmErr)) {
        qDebug() << "VisaChannel:" << rmErr;
        return false;
    }

    SharedInstrument& slot = sharedInstruments()[address];
    if (slot.session != VI_NULL) {
        // ??ref=0 ??????????????????????????? viOpen
        if (slot.timeoutMs != config_.timeoutMs) {
            slot.timeoutMs = config_.timeoutMs;
            viSetAttribute(slot.session, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(config_.timeoutMs));
        }
        ++slot.refCount;
        holdsSharedInst_ = true;
        qDebug() << "VisaChannel: ???????????" << address << "ref=" << slot.refCount;
        return true;
    }

    const QByteArray addr = address.toLatin1();
    const bool gpib = isGpiBResource(address);
    constexpr int kGpiBOpenAttempts = 2;

    for (int attempt = 1; attempt <= (gpib ? kGpiBOpenAttempts : 1); ++attempt) {
        if (gpib)
            waitGpiBReopenCooldown(address);

        ViSession inst = VI_NULL;
        ViStatus openStatus = VI_ERROR_SYSTEM_ERROR;
        // GPIB ????????????VI_NULL ???????????????????????? ABORT??
        if (gpib) {
            openStatus = viOpen(sharedResourceManager(), (ViRsrc)addr.constData(), VI_NULL, VI_NULL, &inst);
        } else {
            openStatus = viOpen(sharedResourceManager(), (ViRsrc)addr.constData(), VI_EXCLUSIVE_LOCK,
                                static_cast<ViUInt32>(qMax(1000, config_.timeoutMs)), &inst);
            if (openStatus < VI_SUCCESS)
                openStatus = viOpen(sharedResourceManager(), (ViRsrc)addr.constData(), VI_NULL, VI_NULL, &inst);
        }
        if (openStatus < VI_SUCCESS) {
            qDebug() << "VisaChannel: ????????? address=" << address << visaStatusText(openStatus)
                     << "attempt=" << attempt << "????????NI-488.2 ?????????????????;
            if (!gpib || attempt >= kGpiBOpenAttempts)
                return false;
            lastGpiBCloseMs().insert(address, QDateTime::currentMSecsSinceEpoch());
            continue;
        }

        configureSessionAfterOpen(inst, address, config_.timeoutMs, config_.asrlBaudRate);
        if (gpib && !warmUpGpiBAfterOpen(inst)) {
            qDebug() << "VisaChannel: GPIB ?????????????????? attempt=" << attempt << "/" << kGpiBOpenAttempts;
            viClose(inst);
            lastGpiBCloseMs().insert(address, QDateTime::currentMSecsSinceEpoch());
            if (attempt >= kGpiBOpenAttempts) {
                qDebug() << "VisaChannel: GPIB ????????? address=" << address
                         << "??I ????????*IDN????????????MAX????????/???/?????;
                return false;
            }
            continue;
        }

        slot.session = inst;
        slot.refCount = 1;
        slot.timeoutMs = config_.timeoutMs;
        slot.openedAtMs = QDateTime::currentMSecsSinceEpoch();
        // GPIB ???????????I/O????????????????
        slot.lastIoAtMs = gpib ? slot.openedAtMs : 0;
        holdsSharedInst_ = true;
        qDebug() << "VisaChannel: ????? << address << "thread=" << QThread::currentThreadId()
                 << "attempt=" << attempt;
        logSharedSessionsLocked(QStringLiteral("ensureConnected"));
        return true;
    }
    return false;
#else
    qDebug() << "VisaChannel: ?????HAVE_NI_VISA";
    return false;
#endif
}

void VisaChannel::discardIdleSharedSession(const QString& resourceAddress) {
#ifdef HAVE_NI_VISA
    QMutexLocker locker(&visaGlobalMutex());
    const QString address = resourceAddress.trimmed();
    if (address.isEmpty())
        return;
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end() || it->session == VI_NULL) {
        if (it != sharedInstruments().end())
            sharedInstruments().erase(it);
        return;
    }
    if (it->refCount > 0) {
        qDebug() << "VisaChannel: ?????????? ref=" << it->refCount << address
                 << "?????viClose?????ref=0 ??????????????;
    }
    if (isGpiBResource(address))
        busSettleDelayMs(50, &visaGlobalMutex());
    else
        flushSessionBuffers(it->session);
    viClose(it->session);
    if (isGpiBResource(address))
        lastGpiBCloseMs().insert(address, QDateTime::currentMSecsSinceEpoch());
    sharedInstruments().erase(it);
    qDebug() << "VisaChannel: ?????????????" << address;
    logSharedSessionsLocked(QStringLiteral("discardIdle"));
#else
    Q_UNUSED(resourceAddress);
#endif
}

void VisaChannel::close() {
#ifdef HAVE_NI_VISA
    QMutexLocker locker(&visaGlobalMutex());
    if (!holdsSharedInst_)
        return;
    holdsSharedInst_ = false;
    const QString address = config_.resourceAddress.trimmed();
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end())
        return;
    --it->refCount;
    if (it->refCount > 0) {
        qDebug() << "VisaChannel: ??????" << address << "ref=" << it->refCount;
        return;
    }
    if (shouldKeepIdleSession(address) && it->session != VI_NULL) {
        qDebug() << "VisaChannel: ????????????????? << address << "ref=0";
        logSharedSessionsLocked(QStringLiteral("closeKeepAlive"));
        return;
    }
    if (it->session != VI_NULL) {
        if (!isGpiBResource(address))
            flushSessionBuffers(it->session);
        else
            busSettleDelayMs(50, &visaGlobalMutex());
        viClose(it->session);
        it->session = VI_NULL;
        if (isGpiBResource(address))
            lastGpiBCloseMs().insert(address, QDateTime::currentMSecsSinceEpoch());
        qDebug() << "VisaChannel: ??????????? << address;
        logSharedSessionsLocked(QStringLiteral("closeHard"));
    }
    sharedInstruments().erase(it);
    // DefaultRM ??????????????? viClose(RM) ????????????????I/O ??? ABORT
#endif
}

bool VisaChannel::write(const QByteArray& data) {
    if (data.isEmpty())
        return false;
    if (!ensureConnected())
        return false;
#ifdef HAVE_NI_VISA
    QMutexLocker locker(&visaGlobalMutex());
    const QString address = config_.resourceAddress.trimmed();
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end() || it->session == VI_NULL) {
        holdsSharedInst_ = false;
        return false;
    }

    const bool gpib = isGpiBResource(address);
    const bool asrl = isAsrlResource(address);
    if (gpib)
        settleGpiBBeforeIoLocked(*it);
    else if (asrl)
        // ????????? RX???????????????????????????
        clearAsrlIoResidue(it->session, "ASRL???");

    qDebug().noquote() << "VISA TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    ViUInt32 writeCount = 0;
    ViStatus status = VI_SUCCESS;
    auto doWrite = [&]() -> bool {
        return gpib ? viWriteGpiBLocked(it->session, data, &writeCount, &status)
                    : viWriteOnce(it->session, data, &writeCount, &status);
    };

    if (!doWrite()) {
        if (gpib) {
            // ??????????????BORT ????????????????????????????????????????
            static const int kSameSessionGapsMs[] = {200, 350, 500};
            bool sameOk = false;
            for (int gap : kSameSessionGapsMs) {
                qDebug() << "VisaChannel: GPIB ??????" << visaStatusText(status) << "retCnt=" << writeCount
                         << "????????? gapMs=" << gap;
                busSettleDelayMs(gap, &visaGlobalMutex());
                writeCount = 0;
                if (doWrite()) {
                    qDebug() << "VisaChannel: GPIB ???????????retCnt=" << writeCount;
                    it->lastIoAtMs = QDateTime::currentMSecsSinceEpoch();
                    busSettleDelayMs(150, &visaGlobalMutex());
                    sameOk = true;
                    break;
                }
            }
            if (sameOk)
                return true;
            qDebug() << "VisaChannel: GPIB ???????????????" << visaStatusText(status)
                     << "????????????";
            invalidateSharedSessionLocked(address, it, &holdsSharedInst_);
            logSharedSessionsLocked(QStringLiteral("writeAbort"));
            locker.unlock();
            const bool reopened = ensureConnected();
            if (!reopened)
                return false;
            locker.relock();
            it = sharedInstruments().find(address);
            if (it == sharedInstruments().end() || it->session == VI_NULL) {
                holdsSharedInst_ = false;
                return false;
            }
            settleGpiBBeforeIoLocked(*it);
            qDebug().noquote() << "VISA TX(reopen):" << QString::fromLatin1(data.toHex(' ').toUpper());
            writeCount = 0;
            if (viWriteGpiBLocked(it->session, data, &writeCount, &status)) {
                qDebug() << "VisaChannel: GPIB ???????????retCnt=" << writeCount;
                it->lastIoAtMs = QDateTime::currentMSecsSinceEpoch();
                busSettleDelayMs(150, &visaGlobalMutex());
                return true;
            }
            qDebug() << "VisaChannel: GPIB ?????????" << visaStatusText(status);
            invalidateSharedSessionLocked(address, it, &holdsSharedInst_);
            return false;
        }
        qDebug() << "VisaChannel: ??????" << visaStatusText(status) << "retCnt=" << writeCount
                 << "?????viClear ?????;
        trySoftViClear(it->session, "??????");
        busSettleDelayMs(50, &visaGlobalMutex());
        writeCount = 0;
        if (!viWriteOnce(it->session, data, &writeCount, &status)) {
            qDebug() << "VisaChannel: ??????????? << visaStatusText(status) << "retCnt=" << writeCount
                     << "??????????????ensureConnected ???";
            invalidateSharedSessionLocked(address, it, &holdsSharedInst_);
            return false;
        }
        qDebug() << "VisaChannel: ????????? retCnt=" << writeCount;
    }
    it->lastIoAtMs = QDateTime::currentMSecsSinceEpoch();
    if (gpib)
        busSettleDelayMs(150, &visaGlobalMutex());
    return true;
#else
    Q_UNUSED(data);
    return false;
#endif
}

bool VisaChannel::read(QByteArray* out, int maxBytes) {
    if (!out || maxBytes <= 0)
        return false;
    out->clear();
    if (!ensureConnected())
        return false;
#ifdef HAVE_NI_VISA
    QMutexLocker locker(&visaGlobalMutex());
    const QString address = config_.resourceAddress.trimmed();
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end() || it->session == VI_NULL) {
        holdsSharedInst_ = false;
        return false;
    }

    const bool gpib = isGpiBResource(address);
    if (gpib)
        settleGpiBBeforeIoLocked(*it);

    QByteArray buffer(maxBytes, '\0');
    ViUInt32 readCount = 0;
    ViStatus status = VI_SUCCESS;
    bool ok = false;
    if (gpib) {
        ok = viReadGpiBLocked(it->session, reinterpret_cast<ViBuf>(buffer.data()),
                              static_cast<ViUInt32>(maxBytes - 1), &readCount, &status);
    } else {
        status = viRead(it->session, reinterpret_cast<ViBuf>(buffer.data()), static_cast<ViUInt32>(maxBytes - 1),
                        &readCount);
        ok = status >= VI_SUCCESS;
    }
    if (!ok) {
        if (gpib) {
            qDebug() << "VisaChannel: GPIB ??????" << visaStatusText(status) << "???????????????";
            return false;
        }
        qDebug() << "VisaChannel: ??????" << visaStatusText(status) << "?????????????????";
        invalidateSharedSessionLocked(address, it, &holdsSharedInst_);
        return false;
    }
    buffer.resize(static_cast<int>(readCount));
    *out = buffer;
    it->lastIoAtMs = QDateTime::currentMSecsSinceEpoch();
    return true;
#else
    Q_UNUSED(maxBytes);
    return false;
#endif
}
