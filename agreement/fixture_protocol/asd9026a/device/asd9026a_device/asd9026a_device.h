#ifndef ASD9026A_DEVICE_H
#define ASD9026A_DEVICE_H

#include <QObject>
#include <QSerialPort>
#include <QString>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

struct Asd9026aAnalogStatus {
    bool outputOn = false;
    /** 已按回包单位换算后的基单位：电压=V，电流=A */
    double voltage = 0.0;
    double current = 0.0;
    quint32 voltageRaw = 0;
    quint32 currentRaw = 0;
    /** 协议单位码：1=V/A 2=m 3=u 4=n（0 按 u） */
    quint8 voltageUnitCode = 0;
    quint8 currentUnitCode = 0;
    int temperatureC = 0;
};

/** ASD9026A 双通道模拟电池：自定义串口帧协议（见 ASD9026A双通道模拟电池协议.xlsx）。 */
class Asd9026aDevice : public QObject {
    Q_OBJECT
  public:
    explicit Asd9026aDevice(QObject* parent = nullptr);
    ~Asd9026aDevice() override;

    bool open(const QString& portName, int baudRate, QString* errorMessage = nullptr);
    void close();
    bool isOpen() const;

    QString portName() const;
    int baudRate() const;

    bool setOutputEnabled(quint8 moduleAddr, bool enabled, QString* errorMessage = nullptr);
    bool configureConstantVoltage(quint8 moduleAddr, double voltageVolts, double currentLimitAmps,
                                  QString* errorMessage = nullptr);
    bool readModuleStatus(quint8 moduleAddr, int* outputMilliVolts, int* outputMilliAmps, QString* errorMessage = nullptr);
    bool readAnalogStatus(quint8 moduleAddr, Asd9026aAnalogStatus* out, QString* errorMessage = nullptr);

  private:
    bool transact(const QByteArray& request, QByteArray* response, QString* errorMessage);

    QSerialPort serial_;
    int requestTimeoutMs_ = 2000;
};

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif

#endif // ASD9026A_DEVICE_H
