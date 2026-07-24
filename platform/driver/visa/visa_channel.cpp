#include "visa_channel.h"

#include <QDebug>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

#ifdef HAVE_NI_VISA
namespace {

// ensureConnected / write / read / close 可重入加锁
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
};

QHash<QString, SharedInstrument>& sharedInstruments() {
    static QHash<QString, SharedInstrument> map;
    return map;
}

bool ensureSharedRmLocked(QString* errOut) {
    ViSession& rm = sharedResourceManager();
    if (rm != VI_NULL)
        return true;
    const ViStatus rmStatus = viOpenDefaultRM(&rm);
    if (rmStatus < VI_SUCCESS) {
        rm = VI_NULL;
        if (errOut)
            *errOut = QStringLiteral("打开 VISA RM 失败 status=%1").arg(static_cast<int>(rmStatus));
        return false;
    }
    return true;
}

} // namespace
#endif

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
    ViChar desc[256] = {0};
    // 无会话时也尽量取描述；失败则只回数字
    if (viStatusDesc(VI_NULL, status, desc) >= VI_SUCCESS && desc[0] != '\0')
        return QString::fromLocal8Bit(desc);
    return QStringLiteral("status=%1").arg(static_cast<int>(status));
}
#endif

bool VisaChannel::ensureConnected() {
#ifdef HAVE_NI_VISA
    QMutexLocker locker(&visaGlobalMutex());
    const QString address = config_.resourceAddress.trimmed();
    if (address.isEmpty()) {
        qDebug() << "VisaChannel: VISA 资源地址为空";
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
        // 已有其它工位/通道打开同一地址：复用句柄，避免多 RM/多会话互抢导致 ABORT
        if (slot.timeoutMs != config_.timeoutMs) {
            slot.timeoutMs = config_.timeoutMs;
            viSetAttribute(slot.session, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(config_.timeoutMs));
        }
        ++slot.refCount;
        holdsSharedInst_ = true;
        qDebug() << "VisaChannel: 复用已打开会话" << address << "ref=" << slot.refCount;
        return true;
    }

    const QByteArray addr = address.toLatin1();
    ViSession inst = VI_NULL;
    // 独占打开：NI-488.2 通讯器等占着时直接失败，避免写到一半被 ABORT
    ViStatus openStatus =
        viOpen(sharedResourceManager(), (ViRsrc)addr.constData(), VI_EXCLUSIVE_LOCK,
               static_cast<ViUInt32>(qMax(1000, config_.timeoutMs)), &inst);
    if (openStatus < VI_SUCCESS) {
        openStatus = viOpen(sharedResourceManager(), (ViRsrc)addr.constData(), VI_NULL, VI_NULL, &inst);
    }
    if (openStatus < VI_SUCCESS) {
        qDebug() << "VisaChannel: 打开设备失败 address=" << address << statusText(openStatus)
                 << "（若刚用过 NI-488.2 通讯器请先关掉该窗口）";
        return false;
    }

    viSetAttribute(inst, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(config_.timeoutMs));
    // 清总线残留，降低首包 viWrite 被中止的概率
    const ViStatus clearStatus = viClear(inst);
    if (clearStatus < VI_SUCCESS)
        qDebug() << "VisaChannel: viClear 警告" << statusText(clearStatus);

    slot.session = inst;
    slot.refCount = 1;
    slot.timeoutMs = config_.timeoutMs;
    holdsSharedInst_ = true;
    qDebug() << "VisaChannel: 已连接" << address;
    return true;
#else
    qDebug() << "VisaChannel: 未启用 HAVE_NI_VISA";
    return false;
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
        qDebug() << "VisaChannel: 释放引用" << address << "ref=" << it->refCount;
        return;
    }
    if (it->session != VI_NULL) {
        viClose(it->session);
        it->session = VI_NULL;
        qDebug() << "VisaChannel: 已关闭仪器会话" << address;
    }
    sharedInstruments().erase(it);
    // DefaultRM 进程内常驻，避免反复 viClose(RM) 把其它通道正在进行的 I/O 打成 ABORT
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

    qDebug().noquote() << "VISA TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    ViUInt32 writeCount = 0;
    const ViStatus status = viWrite(it->session, reinterpret_cast<ViBuf>(const_cast<char*>(data.constData())),
                                    static_cast<ViUInt32>(data.size()), &writeCount);
    if (status < VI_SUCCESS) {
        qDebug() << "VisaChannel: 写入失败" << statusText(status) << "retCnt=" << writeCount;
        // 写入失败不主动拆会话：拆了立刻重开会放大 ABORT；由上层 resetVisaBackend 显式关闭
        return false;
    }
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

    QByteArray buffer(maxBytes, '\0');
    ViUInt32 readCount = 0;
    const ViStatus status =
        viRead(it->session, reinterpret_cast<ViBuf>(buffer.data()), static_cast<ViUInt32>(maxBytes - 1), &readCount);
    if (status < VI_SUCCESS) {
        qDebug() << "VisaChannel: 读取失败" << statusText(status);
        return false;
    }
    buffer.resize(static_cast<int>(readCount));
    *out = buffer;
    return true;
#else
    Q_UNUSED(maxBytes);
    return false;
#endif
}
