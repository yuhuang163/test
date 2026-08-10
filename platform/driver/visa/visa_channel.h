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
        /** 仅 ASRL 生效；须与仪器一致，默认 9600，可由 VisaPower/BaudRate 覆盖 */
        int asrlBaudRate = 9600;
    };

    explicit VisaChannel(QObject* parent = nullptr);
    ~VisaChannel() override;

    void setConfig(const Config& config);
    Config config() const;

    bool ensureConnected();
    /** 释放本通道引用。TCPIP 可保活；GPIB/ASRL 引用归零时真 viClose。 */
    void close();
    bool isOpen() const;

    bool write(const QByteArray& data);
    bool read(QByteArray* out, int maxBytes = 1024);

    /** 正式测开局/测完：强制关闭进程内该地址的空闲 GPIB 句柄。 */
    static void discardIdleSharedSession(const QString& resourceAddress);
    /** 调试：打印进程内共享 VISA 会话 ref/句柄（无独立 VISA 线程，仅主线程+全局锁）。 */
    static void dumpSharedSessions(const QString& tag);
    /**
     * 可泵界面的等待（排除 Socket）。GPIB 会话已打开或即将 viWrite 时禁止用：
     * processEvents 仍可能跑定时器/重入，易把 GPIB 写成 VI_ERROR_ABORT。
     */
    static void pumpDelayMs(int ms);
    /** GPIB 临界区等待：纯 sleep，不泵事件。 */
    static void idleDelayMs(int ms);

  private:
    Config config_;
#ifdef HAVE_NI_VISA
    /** 本通道是否持有共享仪器会话引用（进程内同地址共用一把句柄）。 */
    bool holdsSharedInst_ = false;
    static QString statusText(ViStatus status);
#endif
};

#endif // PLATFORM_VISA_CHANNEL_H
