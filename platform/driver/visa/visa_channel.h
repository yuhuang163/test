#ifndef PLATFORM_VISA_CHANNEL_H
#define PLATFORM_VISA_CHANNEL_H

#include <QByteArray>
#include <QObject>
#include <QString>

// 用相对路径：Cursor/clangd 无 qmake INCLUDEPATH 时也能解析到宏，避免 #ifdef 整段发灰
#include "../../../lib/visa/have_ni_visa.h"
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

    /** 「配置Visa程控电源」开局：强制关闭该地址进程内共享句柄，避免僵死会话占线。 */
    static void discardIdleSharedSession(const QString& resourceAddress);
    /** 调试：打印进程内共享 VISA 会话 ref/句柄。 */
    static void dumpSharedSessions(const QString& tag);
    /** 统一延时：与 test_base::waitWork 同款（processEvents）。 */
    static void waitWork(int ms);

  private:
    Config config_;
#ifdef HAVE_NI_VISA
    /** 本通道是否持有共享仪器会话引用（进程内同地址共用一把句柄）。 */
    bool holdsSharedInst_ = false;
    static QString statusText(ViStatus status);
#endif
};

#endif // PLATFORM_VISA_CHANNEL_H
