#include "visa_channel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QStringList>
#include <QThread>
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

QString resourceKindText(const QString& address) {
    if (isGpiBResource(address))
        return QStringLiteral("GPIB");
    if (isAsrlResource(address))
        return QStringLiteral("ASRL");
    return QStringLiteral("USB/其它");
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

// ===== 以下 worker* 函数与共享表，只允许在 VISA 专用线程里访问 =====

/** worker 侧不直接 qDebug：日志文本回传给调用线程打印。
    日志处理器内部无锁且会读 SETTINGS，多线程同时进不安全。 */
void addLog(QStringList* logs, const QString& text) {
    if (logs)
        logs->append(text);
}

void flushSessionBuffers(ViSession inst) {
    viFlush(inst, VI_WRITE_BUF);
    viFlush(inst, VI_READ_BUF);
}

/** ASRL 写前清 RX，避免串口残留导致「发什么都乱回」。GPIB 不要走这条。 */
void clearAsrlIoResidue(ViSession inst, QStringList* logs) {
    viFlush(inst, static_cast<ViUInt16>(VI_READ_BUF_DISCARD | VI_WRITE_BUF_DISCARD | VI_IO_IN_BUF_DISCARD));
    const ViStatus clearStatus = viClear(inst);
    if (clearStatus < VI_SUCCESS)
        addLog(logs, QStringLiteral("VisaChannel: ASRL viClear 警告 %1").arg(visaStatusText(clearStatus)));
}

void configureSession(ViSession inst, const QString& address, int timeoutMs, int asrlBaudRate, QStringList* logs) {
    // GPIB 超时不宜过长，失败时 viWrite/viRead 会堵住 VISA 线程
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
        addLog(logs, QStringLiteral("VisaChannel: ASRL \"%1\" baud= %2 8N1").arg(address).arg(baud));
        clearAsrlIoResidue(inst, logs);
        // 在 VISA 线程里等待，不能泵事件
        QThread::msleep(50);
    } else {
        // USB/TCPIP 等：打开后软清一次
        const ViStatus clearStatus = viClear(inst);
        if (clearStatus < VI_SUCCESS)
            addLog(logs, QStringLiteral("VisaChannel: 打开后 viClear 警告 %1").arg(visaStatusText(clearStatus)));
    }
}

bool ensureSharedRm(QString* errOut, QStringList* logs) {
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
        addLog(logs, QStringLiteral("VisaChannel: viOpenDefaultRM 警告 %1").arg(visaStatusText(rmStatus)));
    return true;
}

/** 写/读失败后作废句柄，下次 acquire 再开。 */
void invalidateSession(const QString& address, QStringList* logs) {
    addLog(logs, QStringLiteral("VisaChannel: 作废会话开始 address= %1").arg(address));
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end()) {
        addLog(logs, QStringLiteral("VisaChannel: 作废会话 — 共享表无此地址 \"%1\"").arg(address));
        return;
    }
    addLog(logs, QStringLiteral("VisaChannel: 作废会话 — 关闭前 ref= %1 session= %2 lastIoMs= %3 %4")
                     .arg(it->refCount)
                     .arg(it->session != VI_NULL ? QStringLiteral("open") : QStringLiteral("null"))
                     .arg(it->lastIoAtMs)
                     .arg(resourceKindText(address)));
    if (it->session != VI_NULL) {
        if (!isGpiBResource(address)) {
            flushSessionBuffers(it->session);
            addLog(logs, QStringLiteral("VisaChannel: 作废会话 — 已 flush 缓冲 \"%1\"").arg(address));
        } else {
            addLog(logs, QStringLiteral("VisaChannel: 作废会话 — GPIB 跳过 flush，直接 viClose \"%1\"").arg(address));
        }
        const ViStatus closeSt = viClose(it->session);
        it->session = VI_NULL;
        addLog(logs, QStringLiteral("VisaChannel: 已作废会话 %1 %2").arg(address, visaStatusText(closeSt)));
    } else {
        addLog(logs, QStringLiteral("VisaChannel: 作废会话 — 句柄已空，仅从表删除 \"%1\"").arg(address));
    }
    sharedInstruments().erase(it);
    addLog(logs, QStringLiteral("VisaChannel: 作废会话结束 address= %1 表剩余条数= %2")
                     .arg(address)
                     .arg(sharedInstruments().size()));
}

/** alreadyHolding：调用方已持一份引用，会话仍在则直接复用、不重复加引用。 */
bool workerAcquire(const QString& address, int timeoutMs, int asrlBaudRate, bool alreadyHolding, QStringList* logs) {
    if (alreadyHolding) {
        auto held = sharedInstruments().constFind(address);
        if (held != sharedInstruments().constEnd() && held->session != VI_NULL)
            return true;
        addLog(logs, QStringLiteral("VisaChannel: 本实例标记已连接但共享会话失效，将重开 \"%1\"").arg(address));
    }

    QString rmErr;
    if (!ensureSharedRm(&rmErr, logs)) {
        addLog(logs, QStringLiteral("VisaChannel: 连接失败 — %1").arg(rmErr));
        return false;
    }

    SharedInstrument& slot = sharedInstruments()[address];
    if (slot.session != VI_NULL) {
        // 他处已打开（含 ref=0 保活）：只加引用，禁止再 viOpen
        if (slot.timeoutMs != timeoutMs) {
            slot.timeoutMs = timeoutMs;
            const ViStatus tmoSt = viSetAttribute(slot.session, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(timeoutMs));
            if (tmoSt < VI_SUCCESS)
                addLog(logs, QStringLiteral("VisaChannel: 复用会话改超时失败 %1 %2").arg(address, visaStatusText(tmoSt)));
        }
        ++slot.refCount;
        addLog(logs, QStringLiteral("VisaChannel: 复用会话 \"%1\" ref= %2").arg(address).arg(slot.refCount));
        return true;
    }

    const QByteArray addr = address.toLatin1();
    ViSession inst = VI_NULL;
    // 统一 VI_NULL 打开；非 GPIB 失败时再试独占锁（部分 USB 设备需要）
    ViStatus openStatus = viOpen(sharedResourceManager(), (ViRsrc)addr.constData(), VI_NULL, VI_NULL, &inst);
    if (openStatus < VI_SUCCESS && !isGpiBResource(address)) {
        addLog(logs, QStringLiteral("VisaChannel: VI_NULL 打开失败，改试独占锁 %1 %2")
                         .arg(address, visaStatusText(openStatus)));
        openStatus = viOpen(sharedResourceManager(), (ViRsrc)addr.constData(), VI_EXCLUSIVE_LOCK,
                            static_cast<ViUInt32>(qMax(1000, timeoutMs)), &inst);
    }
    if (openStatus < VI_SUCCESS) {
        addLog(logs, QStringLiteral("VisaChannel: 连接失败 — 打开设备失败 address= %1 %2"
                                    "（请核对地址/线缆/驱动，并关闭 NI MAX、其它占用该仪器的程序）")
                         .arg(address, visaStatusText(openStatus)));
        sharedInstruments().remove(address);
        return false;
    }

    configureSession(inst, address, timeoutMs, asrlBaudRate, logs);
    slot.session = inst;
    slot.refCount = 1;
    slot.timeoutMs = timeoutMs;
    slot.lastIoAtMs = 0;
    addLog(logs, QStringLiteral("VisaChannel: 已连接 \"%1\" %2").arg(address, resourceKindText(address)));
    return true;
}

void workerRelease(const QString& address, QStringList* logs) {
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end()) {
        addLog(logs, QStringLiteral("VisaChannel: close 时共享表无此地址 \"%1\"").arg(address));
        return;
    }
    --it->refCount;
    if (it->refCount > 0) {
        addLog(logs, QStringLiteral("VisaChannel: 释放引用 \"%1\" ref= %2").arg(address).arg(it->refCount));
        return;
    }
    // TCPIP 可保活；GPIB/ASRL 引用归零则真 viClose
    if (shouldKeepIdleSession(address) && it->session != VI_NULL) {
        addLog(logs, QStringLiteral("VisaChannel: 保活空闲会话 \"%1\"").arg(address));
        return;
    }
    if (it->session != VI_NULL) {
        if (!isGpiBResource(address))
            flushSessionBuffers(it->session);
        const ViStatus closeSt = viClose(it->session);
        if (closeSt < VI_SUCCESS)
            addLog(logs, QStringLiteral("VisaChannel: viClose 失败 %1 %2").arg(address, visaStatusText(closeSt)));
        it->session = VI_NULL;
        addLog(logs, QStringLiteral("VisaChannel: 已关闭会话 \"%1\"").arg(address));
    }
    sharedInstruments().erase(it);
}

void workerDiscard(const QString& address, QStringList* logs) {
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end() || it->session == VI_NULL) {
        if (it != sharedInstruments().end())
            sharedInstruments().erase(it);
        return;
    }
    if (it->refCount > 0)
        addLog(logs, QStringLiteral("VisaChannel: discard 时仍有引用 ref= %1 \"%2\"").arg(it->refCount).arg(address));
    if (!isGpiBResource(address))
        flushSessionBuffers(it->session);
    viClose(it->session);
    sharedInstruments().erase(it);
    addLog(logs, QStringLiteral("VisaChannel: 配置步作废会话 \"%1\"").arg(address));
}

qint64 workerLastIoAtMs(const QString& address) {
    auto it = sharedInstruments().constFind(address);
    return it == sharedInstruments().constEnd() ? 0 : it->lastIoAtMs;
}

/** ok=本次 I/O 成功；sessionDropped=会话已被作废，调用方须清掉自己的引用标记。 */
struct IoOutcome {
    bool ok = false;
    bool sessionDropped = false;
};

IoOutcome workerWrite(const QString& address, const QByteArray& data, QStringList* logs) {
    IoOutcome outcome;
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end() || it->session == VI_NULL) {
        addLog(logs, QStringLiteral("VisaChannel: 写入失败 — 共享会话不存在或已空 address= %1 inMap= %2")
                         .arg(address)
                         .arg(it != sharedInstruments().end() ? QStringLiteral("true") : QStringLiteral("false")));
        outcome.sessionDropped = true;
        return outcome;
    }

    if (isAsrlResource(address))
        clearAsrlIoResidue(it->session, logs);

    ViUInt32 writeCount = 0;
    const ViStatus status = viWrite(it->session, reinterpret_cast<ViBuf>(const_cast<char*>(data.constData())),
                                    static_cast<ViUInt32>(data.size()), &writeCount);
    if (status < VI_SUCCESS) {
        addLog(logs, QStringLiteral("VisaChannel: 写入失败 address= %1 %2 retCnt= %3 / %4"
                                    "（常见：ABORT=总线忙/被占，TMO=超时，NLISTENERS=无仪器）")
                         .arg(address, visaStatusText(status))
                         .arg(writeCount)
                         .arg(data.size()));
        invalidateSession(address, logs);
        outcome.sessionDropped = true;
        return outcome;
    }
    if (writeCount != static_cast<ViUInt32>(data.size())) {
        addLog(logs, QStringLiteral("VisaChannel: 写入长度不完整 address= %1 retCnt= %2 expect= %3")
                         .arg(address)
                         .arg(writeCount)
                         .arg(data.size()));
    }
    it->lastIoAtMs = QDateTime::currentMSecsSinceEpoch();
    outcome.ok = true;
    return outcome;
}

IoOutcome workerRead(const QString& address, int maxBytes, QByteArray* out, QStringList* logs) {
    IoOutcome outcome;
    auto it = sharedInstruments().find(address);
    if (it == sharedInstruments().end() || it->session == VI_NULL) {
        addLog(logs, QStringLiteral("VisaChannel: 读取失败 — 共享会话不存在或已空 address= %1").arg(address));
        outcome.sessionDropped = true;
        return outcome;
    }

    QByteArray buffer(maxBytes, '\0');
    ViUInt32 readCount = 0;
    const ViStatus status = viRead(it->session, reinterpret_cast<ViBuf>(buffer.data()),
                                   static_cast<ViUInt32>(maxBytes - 1), &readCount);
    if (status < VI_SUCCESS) {
        addLog(logs, QStringLiteral("VisaChannel: 读取失败 address= %1 %2 readCnt= %3"
                                    "（常见：TMO=无回包/超时，ABORT=总线中断）")
                         .arg(address, visaStatusText(status))
                         .arg(readCount));
        // GPIB 读失败先保会话，由上层再试；其它接口作废重开
        if (!isGpiBResource(address)) {
            addLog(logs, QStringLiteral("VisaChannel: 非 GPIB 读失败，作废会话以便重开 \"%1\"").arg(address));
            invalidateSession(address, logs);
            outcome.sessionDropped = true;
        } else {
            addLog(logs, QStringLiteral("VisaChannel: GPIB 读失败，保留会话由上层再试 \"%1\"").arg(address));
        }
        return outcome;
    }
    if (readCount == 0) {
        addLog(logs, QStringLiteral("VisaChannel: 读取失败 — 回包长度为 0 address= %1 %2")
                         .arg(address, visaStatusText(status)));
        return outcome;
    }
    buffer.resize(static_cast<int>(readCount));
    *out = buffer;
    it->lastIoAtMs = QDateTime::currentMSecsSinceEpoch();
    addLog(logs, QStringLiteral("VISA RX: %1 addr= %2 bytes= %3")
                     .arg(QString::fromLatin1(buffer.toHex(' ').toUpper()), address)
                     .arg(readCount));
    outcome.ok = true;
    return outcome;
}

/** 退出前在 VISA 线程里收干净：先关全部仪器会话，再关 DefaultRM。 */
void workerShutdown() {
    for (auto it = sharedInstruments().begin(); it != sharedInstruments().end(); ++it) {
        if (it->session == VI_NULL)
            continue;
        if (!isGpiBResource(it.key()))
            flushSessionBuffers(it->session);
        viClose(it->session);
        it->session = VI_NULL;
    }
    sharedInstruments().clear();
    ViSession& rm = sharedResourceManager();
    if (rm != VI_NULL) {
        viClose(rm);
        rm = VI_NULL;
    }
}

/** VISA 专用线程：进程内所有 visa.h 调用都在这条线程上串行执行。
    与 GUI 线程（dongle 串口事件、Windows 消息泵、processEvents）彻底隔开，
    避免同步 I/O 被打断，也避免 VISA 调用之间被回调重入。 */
class VisaThread {
  public:
    static VisaThread& instance() {
        static VisaThread holder;
        return holder;
    }

    /** 懒启动；返回住在 VISA 线程里的投递对象，线程已收尾时返回 nullptr。 */
    QObject* context() {
        if (stopped_)
            return nullptr;
        if (!context_) {
            thread_ = new QThread;
            thread_->setObjectName(QStringLiteral("VisaIo"));
            context_ = new QObject;
            context_->moveToThread(thread_);
            thread_->start();
            if (QCoreApplication* app = QCoreApplication::instance()) {
                QObject::connect(app, &QCoreApplication::aboutToQuit, app,
                                 [] { VisaThread::instance().shutdown(); });
            }
            qDebug() << "VisaChannel: VISA 专用线程已启动";
        }
        return context_;
    }

    void shutdown() {
        if (stopped_)
            return;
        stopped_ = true;
        if (!context_)
            return;
        QMetaObject::invokeMethod(context_, [] { workerShutdown(); }, Qt::BlockingQueuedConnection);
        thread_->quit();
        thread_->wait(3000);
        delete context_;
        context_ = nullptr;
        delete thread_;
        thread_ = nullptr;
        qDebug() << "VisaChannel: VISA 专用线程已退出";
    }

  private:
    VisaThread() = default;

    QThread* thread_ = nullptr;
    QObject* context_ = nullptr;
    bool stopped_ = false;
};

/** 把一段纯 VISA 调用同步丢到 VISA 线程执行；线程不可用时返回 false（此时 fn 未执行）。 */
template <typename Fn>
bool runOnVisaThread(Fn fn) {
    QObject* ctx = VisaThread::instance().context();
    if (!ctx)
        return false;
    if (QThread::currentThread() == ctx->thread()) {
        fn();
        return true;
    }
    return QMetaObject::invokeMethod(ctx, fn, Qt::BlockingQueuedConnection);
}

void flushLogs(const QStringList& logs) {
    for (const QString& line : logs)
        qDebug().noquote() << line;
}

/** USB-GPIB 上连续 I/O 间隔过短易 ABORT，补到至少 200ms。
    间隔在调用线程用 waitWork 等，期间 dongle 串口等事件照常处理。 */
void settleGpiBBeforeIo(const QString& address) {
    qint64 lastIoAtMs = 0;
    if (!runOnVisaThread([&] { lastIoAtMs = workerLastIoAtMs(address); }))
        return;
    if (lastIoAtMs <= 0)
        return;
    const qint64 sinceIo = QDateTime::currentMSecsSinceEpoch() - lastIoAtMs;
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

    QStringList logs;
    bool ok = false;
    const bool dispatched = runOnVisaThread([&] {
        ok = workerAcquire(address, config_.timeoutMs, config_.asrlBaudRate, holdsSharedInst_, &logs);
    });
    flushLogs(logs);
    if (!dispatched) {
        qDebug() << "VisaChannel: 连接失败 — VISA 线程不可用" << address;
        holdsSharedInst_ = false;
        return false;
    }
    holdsSharedInst_ = ok;
    return ok;
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
    QStringList logs;
    runOnVisaThread([&] { workerDiscard(address, &logs); });
    flushLogs(logs);
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
    QStringList logs;
    runOnVisaThread([&] { workerRelease(address, &logs); });
    flushLogs(logs);
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
    const bool gpib = isGpiBResource(address);
    if (gpib)
        settleGpiBBeforeIo(address);

    qDebug().noquote() << "VISA TX:" << QString::fromLatin1(data.toHex(' ').toUpper())
                       << "addr=" << address << "bytes=" << data.size();

    QStringList logs;
    IoOutcome outcome;
    const bool dispatched = runOnVisaThread([&] { outcome = workerWrite(address, data, &logs); });
    flushLogs(logs);
    if (!dispatched) {
        qDebug() << "VisaChannel: 写入失败 — VISA 线程不可用" << address;
        holdsSharedInst_ = false;
        return false;
    }
    if (outcome.sessionDropped)
        holdsSharedInst_ = false;
    if (!outcome.ok)
        return false;
    // 写后再留一点间隔，降低紧接下一条 ABORT 概率
    if (gpib)
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
    if (isGpiBResource(address))
        settleGpiBBeforeIo(address);

    QStringList logs;
    IoOutcome outcome;
    const bool dispatched = runOnVisaThread([&] { outcome = workerRead(address, maxBytes, out, &logs); });
    flushLogs(logs);
    if (!dispatched) {
        qDebug() << "VisaChannel: 读取失败 — VISA 线程不可用" << address;
        holdsSharedInst_ = false;
        return false;
    }
    if (outcome.sessionDropped)
        holdsSharedInst_ = false;
    return outcome.ok;
#else
    Q_UNUSED(maxBytes);
    qDebug() << "VisaChannel: 读取失败 — 未启用 HAVE_NI_VISA";
    return false;
#endif
}
