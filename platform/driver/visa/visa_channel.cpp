#include "visa_channel.h"

#include <QDebug>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
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
    return visaInst_ != VI_NULL;
#else
    return false;
#endif
}

bool VisaChannel::ensureConnected() {
#ifdef HAVE_NI_VISA
    if (visaInst_ != VI_NULL) {
        return true;
    }
#endif
    if (config_.resourceAddress.trimmed().isEmpty()) {
        qDebug() << "VisaChannel: VISA 资源地址为空";
        return false;
    }
#ifdef HAVE_NI_VISA
    if (visaRm_ == VI_NULL) {
        const ViStatus rmStatus = viOpenDefaultRM(&visaRm_);
        if (rmStatus < VI_SUCCESS) {
            qDebug() << "VisaChannel: 打开 VISA RM 失败 status=" << rmStatus;
            visaRm_ = VI_NULL;
            return false;
        }
    }
    const QByteArray addr = config_.resourceAddress.trimmed().toLocal8Bit();
    const ViStatus openStatus = viOpen(visaRm_, (ViRsrc)addr.constData(), VI_NULL, VI_NULL, &visaInst_);
    if (openStatus < VI_SUCCESS) {
        qDebug() << "VisaChannel: 打开设备失败 address=" << config_.resourceAddress << "status=" << openStatus;
        visaInst_ = VI_NULL;
        return false;
    }
    viSetAttribute(visaInst_, VI_ATTR_TMO_VALUE, static_cast<ViAttrState>(config_.timeoutMs));
    qDebug() << "VisaChannel: 已连接" << config_.resourceAddress;
    return true;
#else
    qDebug() << "VisaChannel: 未启用 HAVE_NI_VISA";
    return false;
#endif
}

void VisaChannel::close() {
#ifdef HAVE_NI_VISA
    if (visaInst_ != VI_NULL) {
        viClose(visaInst_);
        visaInst_ = VI_NULL;
    }
    if (visaRm_ != VI_NULL) {
        viClose(visaRm_);
        visaRm_ = VI_NULL;
    }
#endif
}

bool VisaChannel::write(const QByteArray& data) {
    if (!ensureConnected() || data.isEmpty()) {
        return false;
    }
#ifdef HAVE_NI_VISA
    qDebug().noquote() << "VISA TX:" << QString::fromLatin1(data.toHex(' ').toUpper());
    ViUInt32 writeCount = 0;
    const ViStatus status =
        viWrite(visaInst_, reinterpret_cast<ViBuf>(const_cast<char*>(data.constData())),
                static_cast<ViUInt32>(data.size()), &writeCount);
    if (status < VI_SUCCESS) {
        qDebug() << "VisaChannel: 写入失败 status=" << status;
        return false;
    }
    return true;
#else
    Q_UNUSED(data);
    return false;
#endif
}

bool VisaChannel::read(QByteArray* out, int maxBytes) {
    if (!out || maxBytes <= 0) {
        return false;
    }
    out->clear();
    if (!ensureConnected()) {
        return false;
    }
#ifdef HAVE_NI_VISA
    QByteArray buffer(maxBytes, '\0');
    ViUInt32 readCount = 0;
    const ViStatus status =
        viRead(visaInst_, reinterpret_cast<ViBuf>(buffer.data()), static_cast<ViUInt32>(maxBytes - 1), &readCount);
    if (status < VI_SUCCESS) {
        qDebug() << "VisaChannel: 读取失败 status=" << status;
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
