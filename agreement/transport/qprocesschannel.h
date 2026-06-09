#ifndef QPROCESSCHANNEL_H
#define QPROCESSCHANNEL_H

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QQueue>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <functional>

class QProcessChannel : public QObject {
    Q_OBJECT
  public:
    using Callback = std::function<void(const QString&, qint64)>;

    explicit QProcessChannel(QObject* parent = nullptr);
    ~QProcessChannel() override;

    bool start(const QString& program, const QStringList& args, int startTimeoutMs = 3000);
    void stop(const QString& exitCommand = QStringLiteral("exit"));

    void setInitCommand(const QByteArray& initCommand);
    void setTxPrefix(const QString& prefix);
    void setRxPrefix(const QString& prefix);
    void setRestartOnTimeout(bool enabled, int restartStartTimeoutMs = 3000);

    void sendCommand(const QString& cmd, Callback callback, qint64 timeoutMs = 3000);

  private slots:
    void onReadyRead();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void checkTimeout();

  private:
    struct CmdItem {
        QString command;
        QString endMark;
        QElapsedTimer timer;
        Callback callback;
        qint64 timeoutMs = 3000;
    };

    void processNextCommand();
    bool restartProcess();
    static QString buildEndMark();

    QProcess* process_ = nullptr;
    QQueue<CmdItem> queue_;
    QString buffer_;
    bool isProcessing_ = false;
    QTimer timeoutTimer_;
    QByteArray initCommand_;
    QString program_;
    QStringList args_;
    int startTimeoutMs_ = 3000;
    bool restartOnTimeout_ = false;
    int restartStartTimeoutMs_ = 3000;
    QString txPrefix_ = QStringLiteral("PROC TX:");
    QString rxPrefix_ = QStringLiteral("PROC RX:");
};

#endif // QPROCESSCHANNEL_H
