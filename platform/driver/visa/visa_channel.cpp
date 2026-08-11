#include "visa_channel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QTime>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

#ifdef HAVE_NI_VISA
namespace {

/** 进程内唯一 DefaultRM，避免反复 viOpenDefaultRM / viClose(RM)。 */
ViSession& sharedResourceManager() {
    static ViSession rm = VI_NULL;
    return rm;
}

/** 同地址多通道共用一把仪器会话（引用计数）。 */
struct SharedInstrument {
    ViSession session = VI_NULL;
    int refCount = 0;
    int timeoutMs = 3000;
    qint64 lastIoAtMs = 0; // 上次成功 I/O 时刻，供 GPIB 写间隔
};

QHash<QString, SharedInstrument>& sharedInstruments() {
    static QHash<QString, SharedInstrument> map;
    return map;
}

bool isGpiBResource(const QString& address) {
    return address.startsWith(QStringLiteral("GPIB"), Qt::CaseInsensitive);
}

bool isAsrlResource(const QString& address) {
    return address.startsWith(QStringLiteral("ASRL"), Qt::CaseInsensitive);
}

/** TCPIP 引用归零可保活；GPIB/ASRL 须真关，否则易占线/残留。 */
bool shouldKeepIdleSession(const QString& address) {
    return address.startsWith(QStringLiteral("TCPIP"), Qt::CaseInsensitive);
}

QString visaStatusText(ViStatus status) {
    ViChar desc[256] = {0};
    QString text;
    if (viStatusDesc(VI_NULL, status, desc) >= VI_SUCCESS && desc[0] != '\0')
        text = QString::fromLocal8Bit(desc);
    else
        text = QStringLiteral("status=%1").arg(static_cast<int>(status));
    // 同时打十六进制，便于对照 visa.h 的 VI_ERROR_*
    return QStringLiteral("%1 (0x%2/%3)")
        .arg(text)
        .arg(static_cast<quint32>(status), 8, 16, QLatin1Char('0'))
        .arg(static_cast<int>(status));
}

void flushSessionBuffers(ViSession inst) {
    viFlush(inst, VI_WRITE_BUF);
    viFlush(inst, VI_READ_BUF);
}

/** ASRL 写前清 RX，避免串口残留导致「发什么都乱回」。GPIB 不要走这条。 */
void clearAsrlIoResidue(ViSession inst) {
    viFlush(inst, static_cast<ViUInt16>(VI_READ_BUF_DISCARD | VI_WRITE_BUF_DISCARD | VI_IO_IN_BUF_DISCARD));
    const ViStatus clearStatus = viClear(inst);
    if (clearStatus < VI_SUCCESS)
        qDebug().noquote() << "VisaChannel: ASRL viClear 警告" << visaStatusText(clearStatus);
}

void configureSession(ViSession inst, const QString& address, int timeoutMs, int asrlBaudRate) {
    // GPIB 超时不宜过长，失败时 viWrite/viRead 会堵主线程
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
        qDebug() << "VisaChannel: ASRL" << address << "baud=" << baud << "8N1";
        clearAsrlIoResidue(inst);
        VisaChannel::waitWork(50);
    } else {
        // USB/TCPIP 等：打开后软清一次
        const ViStatus clearStatus = viClear(inst);
        if (clearStatus < VI_SUCCESS)
            qDebug().noquote() << "VisaChannel: 打开后 viClear 警告" << visaStatusText(clearStatus);
    }
}

bool ensureSharedRm(QString* errOut) {
    ViSession& rm = sharedResourceManager();
    if (rm != VI_NULL)
        return true;
    const ViStatus rmStatus = viOpenDefaultRM(&rm);
    if (rmStatus < VI_SUCCESS) {
        rm = VI_NULL;
        if (errOut)
            *errOut = QStringLiteral("打开 VISA RM 失败 %1").arg(visaStatusText(rmStatus));
        return false;
    }
    // 警告仍可用，但记日志（如配置未加载）
    if (rmStatus > VI_SUCCESS)
        qDebug().noquote() << "VisaChannel: viOpenDefaultRM 警告" << visaStatusText(rmStatus);
    return true;
}

/** 写/读失败后作废句柄，下次 ensureConnected 再开。 */
void invalidateSession(const QString& address, bool* holdsSharedInstFlag) {
    qDebug().noquote() << "VisaChannel: 作废会话开始 address=" << address
                       << "holdsFlag=" << (holdsSharedInstFlag && *holdsSharedInstFlag);
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end()) {
        qDebug().noquote() << "VisaChannel: 作废会话 — 共享表无此地址" << address;
        if (holdsSharedInstFlag)
            *holdsSharedInstFlag = false;
        return;
    }
    const int refBefore = it->refCount;
    const bool sessionOpen = (it->session != VI_NULL);
    qDebug().noquote() << "VisaChannel: 作废会话 — 关闭前 ref=" << refBefore
                       << "session=" << (sessionOpen ? "open" : "null")
                       << "lastIoMs=" << it->lastIoAtMs
                       << (isGpiBResource(address) ? "GPIB" : (isAsrlResource(address) ? "ASRL" : "USB/其它"));
    if (it->session != VI_NULL) {
        if (!isGpiBResource(address)) {
            flushSessionBuffers(it->session);
            qDebug() << "VisaChannel: 作废会话 — 已 flush 缓冲" << address;
        } else {
            qDebug() << "VisaChannel: 作废会话 — GPIB 跳过 flush，直接 viClose" << address;
        }
        const ViStatus closeSt = viClose(it->session);
        it->session = VI_NULL;
        qDebug().noquote() << "VisaChannel: 已作废会话" << address << visaStatusText(closeSt);
    } else {
        qDebug() << "VisaChannel: 作废会话 — 句柄已空，仅从表删除" << address;
    }
    sharedInstruments().erase(it);
    if (holdsSharedInstFlag)
        *holdsSharedInstFlag = false;
    qDebug().noquote() << "VisaChannel: 作废会话结束 address=" << address
                       << "表剩余条数=" << sharedInstruments().size();
}

/** USB-GPIB 上连续写间隔过短易 ABORT，写前补到至少 200ms。 */
void settleGpiBBeforeIo(SharedInstrument& slot) {
    if (slot.lastIoAtMs <= 0)
        return;
    const qint64 sinceIo = QDateTime::currentMSecsSinceEpoch() - slot.lastIoAtMs;
    constexpr qint64 kMinGapMs = 200;
    if (sinceIo < kMinGapMs)
        VisaChannel::waitWork(static_cast<int>(kMinGapMs - sinceIo));
}

} // namespace
#endif

void VisaChannel::waitWork(int ms) {
    // 与 test_base::waitWork 同款：泵事件等待
    if (ms <= 0)
        return;
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}

VisaChannel::VisaChannel(QObject* parent) : QObject(parent) {
}

VisaChannel::~VisaChannel() {
    close();
}

void VisaChannel::setConfig(const Config& config) {
    // 换地址须先释放旧引用，避免挂在错误会话上
    if (config_.resourceAddress != config.resourceAddress)
        close();
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
    const QString address = config_.resourceAddress.trimmed();
    if (address.isEmpty()) {
        qDebug() << "VisaChannel: 连接失败 — VISA 资源地址为空";
        return false;
    }
    // 本实例已持引用且共享表仍有效 → 直接复用
    if (holdsSharedInst_) {
        auto it = sharedInstruments().find(address);
        if (it != sharedInstruments().end() && it->session != VI_NULL)
            return true;
        qDebug() << "VisaChannel: 本实例标记已连接但共享会话失效，将重开" << address;
        holdsSharedInst_ = false;
    }

    QString rmErr;
    if (!ensureSharedRm(&rmErr)) {
        qDebug().noquote() << "VisaChannel: 连接失败 —" << rmErr;
        return false;
    }

    SharedInstrument& slot = sharedInstruments()[address];
    if (slot.session != VI_NULL) {
        // 他处已打开（含 ref=0 保活）：只加引用，禁止再 viOpen
        if (slot.timeoutMs != config_.timeoutMs) {
            slot.timeoutMs = config_.timeoutMs;
            const ViStatus tmoSt =
                viSetAttribute(slot.session, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(config_.timeoutMs));
            if (tmoSt < VI_SUCCESS)
                qDebug().noquote() << "VisaChannel: 复用会话改超时失败" << address << visaStatusText(tmoSt);
        }
        ++slot.refCount;
        holdsSharedInst_ = true;
        qDebug() << "VisaChannel: 复用会话" << address << "ref=" << slot.refCount;
        return true;
    }

    const QByteArray addr = address.toLatin1();
    ViSession inst = VI_NULL;
    // 统一 VI_NULL 打开；非 GPIB 失败时再试独占锁（部分 USB 设备需要）
    ViStatus openStatus =
        viOpen(sharedResourceManager(), (ViRsrc)addr.constData(), VI_NULL, VI_NULL, &inst);
    if (openStatus < VI_SUCCESS && !isGpiBResource(address)) {
        qDebug().noquote() << "VisaChannel: VI_NULL 打开失败，改试独占锁" << address
                           << visaStatusText(openStatus);
        openStatus = viOpen(sharedResourceManager(), (ViRsrc)addr.constData(), VI_EXCLUSIVE_LOCK,
                            static_cast<ViUInt32>(qMax(1000, config_.timeoutMs)), &inst);
    }
    if (openStatus < VI_SUCCESS) {
        qDebug().noquote() << "VisaChannel: 连接失败 — 打开设备失败 address=" << address
                           << visaStatusText(openStatus)
                           << "（请核对地址/线缆/驱动，并关闭 NI MAX、其它占用该仪器的程序）";
        sharedInstruments().remove(address);
        return false;
    }

    configureSession(inst, address, config_.timeoutMs, config_.asrlBaudRate);
    slot.session = inst;
    slot.refCount = 1;
    slot.timeoutMs = config_.timeoutMs;
    slot.lastIoAtMs = 0;
    holdsSharedInst_ = true;
    qDebug() << "VisaChannel: 已连接" << address
             << (isGpiBResource(address) ? "GPIB" : (isAsrlResource(address) ? "ASRL" : "USB/其它"));
    return true;
#else
    qDebug() << "VisaChannel: 连接失败 — 未启用 HAVE_NI_VISA（工程未链 NI-VISA）";
    return false;
#endif
}

void VisaChannel::discardIdleSharedSession(const QString& resourceAddress) {
#ifdef HAVE_NI_VISA
    // 配置Visa程控电源开局：强制清掉该地址会话（含仍有引用时），避免僵死句柄占线
    const QString address = resourceAddress.trimmed();
    if (address.isEmpty())
        return;
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end() || it->session == VI_NULL) {
        if (it != sharedInstruments().end())
            sharedInstruments().erase(it);
        return;
    }
    if (it->refCount > 0)
        qDebug() << "VisaChannel: discard 时仍有引用 ref=" << it->refCount << address;
    if (!isGpiBResource(address))
        flushSessionBuffers(it->session);
    viClose(it->session);
    sharedInstruments().erase(it);
    qDebug() << "VisaChannel: 配置步作废会话" << address;
#else
    Q_UNUSED(resourceAddress);
#endif
}

void VisaChannel::close() {
#ifdef HAVE_NI_VISA
    if (!holdsSharedInst_)
        return;
    holdsSharedInst_ = false;
    const QString address = config_.resourceAddress.trimmed();
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end()) {
        qDebug() << "VisaChannel: close 时共享表无此地址" << address;
        return;
    }
    --it->refCount;
    if (it->refCount > 0) {
        qDebug() << "VisaChannel: 释放引用" << address << "ref=" << it->refCount;
        return;
    }
    // TCPIP 可保活；GPIB/ASRL 引用归零则真 viClose
    if (shouldKeepIdleSession(address) && it->session != VI_NULL) {
        qDebug() << "VisaChannel: 保活空闲会话" << address;
        return;
    }
    if (it->session != VI_NULL) {
        if (!isGpiBResource(address))
            flushSessionBuffers(it->session);
        const ViStatus closeSt = viClose(it->session);
        if (closeSt < VI_SUCCESS)
            qDebug().noquote() << "VisaChannel: viClose 失败" << address << visaStatusText(closeSt);
        it->session = VI_NULL;
        qDebug() << "VisaChannel: 已关闭会话" << address;
    }
    sharedInstruments().erase(it);
#endif
}

bool VisaChannel::write(const QByteArray& data) {
    if (data.isEmpty()) {
        qDebug() << "VisaChannel: 写入失败 — 数据为空";
        return false;
    }
    if (!ensureConnected()) {
        qDebug() << "VisaChannel: 写入失败 — 未连接 address=" << config_.resourceAddress;
        return false;
    }
#ifdef HAVE_NI_VISA
    const QString address = config_.resourceAddress.trimmed();
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end() || it->session == VI_NULL) {
        qDebug() << "VisaChannel: 写入失败 — 共享会话不存在或已空 address=" << address
                 << "inMap=" << (it != sharedInstruments().end());
        holdsSharedInst_ = false;
        return false;
    }

    if (isGpiBResource(address))
        settleGpiBBeforeIo(*it);
    else if (isAsrlResource(address))
        clearAsrlIoResidue(it->session);

    qDebug().noquote() << "VISA TX:" << QString::fromLatin1(data.toHex(' ').toUpper())
                       << "addr=" << address << "bytes=" << data.size();
    ViUInt32 writeCount = 0;
    const ViStatus status = viWrite(it->session, reinterpret_cast<ViBuf>(const_cast<char*>(data.constData())),
                                    static_cast<ViUInt32>(data.size()), &writeCount);
    if (status < VI_SUCCESS) {
        qDebug().noquote() << "VisaChannel: 写入失败 address=" << address << visaStatusText(status)
                           << "retCnt=" << writeCount << "/" << data.size()
                           << "（常见：ABORT=总线忙/被占，TMO=超时，NLISTENERS=无仪器）";
        invalidateSession(address, &holdsSharedInst_);
        return false;
    }
    if (writeCount != static_cast<ViUInt32>(data.size())) {
        qDebug() << "VisaChannel: 写入长度不完整 address=" << address << "retCnt=" << writeCount
                 << "expect=" << data.size();
    }
    it->lastIoAtMs = QDateTime::currentMSecsSinceEpoch();
    // 写后再留一点间隔，降低紧接下一条 ABORT 概率
    if (isGpiBResource(address))
        VisaChannel::waitWork(150);
    return true;
#else
    Q_UNUSED(data);
    qDebug() << "VisaChannel: 写入失败 — 未启用 HAVE_NI_VISA";
    return false;
#endif
}

bool VisaChannel::read(QByteArray* out, int maxBytes) {
    if (!out) {
        qDebug() << "VisaChannel: 读取失败 — out 为空指针";
        return false;
    }
    if (maxBytes <= 0) {
        qDebug() << "VisaChannel: 读取失败 — maxBytes 非法" << maxBytes;
        return false;
    }
    out->clear();
    if (!ensureConnected()) {
        qDebug() << "VisaChannel: 读取失败 — 未连接 address=" << config_.resourceAddress;
        return false;
    }
#ifdef HAVE_NI_VISA
    const QString address = config_.resourceAddress.trimmed();
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end() || it->session == VI_NULL) {
        qDebug() << "VisaChannel: 读取失败 — 共享会话不存在或已空 address=" << address;
        holdsSharedInst_ = false;
        return false;
    }

    if (isGpiBResource(address))
        settleGpiBBeforeIo(*it);

    QByteArray buffer(maxBytes, '\0');
    ViUInt32 readCount = 0;
    const ViStatus status =
        viRead(it->session, reinterpret_cast<ViBuf>(buffer.data()), static_cast<ViUInt32>(maxBytes - 1),
               &readCount);
    if (status < VI_SUCCESS) {
        qDebug().noquote() << "VisaChannel: 读取失败 address=" << address << visaStatusText(status)
                           << "readCnt=" << readCount
                           << "（常见：TMO=无回包/超时，ABORT=总线中断）";
        // GPIB 读失败先保会话，由上层再试；其它接口作废重开
        if (!isGpiBResource(address)) {
            qDebug() << "VisaChannel: 非 GPIB 读失败，作废会话以便重开" << address;
            invalidateSession(address, &holdsSharedInst_);
        } else {
            qDebug() << "VisaChannel: GPIB 读失败，保留会话由上层再试" << address;
        }
        return false;
    }
    if (readCount == 0) {
        qDebug() << "VisaChannel: 读取失败 — 回包长度为 0 address=" << address
                 << visaStatusText(status);
        return false;
    }
    buffer.resize(static_cast<int>(readCount));
    *out = buffer;
    it->lastIoAtMs = QDateTime::currentMSecsSinceEpoch();
    qDebug().noquote() << "VISA RX:" << QString::fromLatin1(buffer.toHex(' ').toUpper())
                       << "addr=" << address << "bytes=" << readCount;
    return true;
#else
    Q_UNUSED(maxBytes);
    qDebug() << "VisaChannel: 读取失败 — 未启用 HAVE_NI_VISA";
    return false;
#endif
}
