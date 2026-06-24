#ifndef PLATFORM_VISA_CHANNEL_H
#define PLATFORM_VISA_CHANNEL_H

#include <QByteArray>
#include <QObject>
#include <QString>

#ifdef HAVE_NI_VISA
#include <visa.h>
#endif

/** 平台 VISA 驱动：资源打开/关闭、viWrite/viRead 字节 I/O，无 SCPI 行语义。 */
class VisaChannel : public QObject {
    Q_OBJECT

  public:
    struct Config {
        QString resourceAddress;
        int timeoutMs = 3000;
    };

    explicit VisaChannel(QObject* parent = nullptr);
    ~VisaChannel() override;

    void setConfig(const Config& config);
    Config config() const;

    bool ensureConnected();
    void close();
    bool isOpen() const;

    bool write(const QByteArray& data);
    bool read(QByteArray* out, int maxBytes = 1024);

  private:
    Config config_;
#ifdef HAVE_NI_VISA
    ViSession visaRm_ = VI_NULL;
    ViSession visaInst_ = VI_NULL;
#endif
};

#endif // PLATFORM_VISA_CHANNEL_H
