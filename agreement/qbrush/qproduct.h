#ifndef QPRODUCT_H
#define QPRODUCT_H



#include <QMessageBox>
#include <QObject>
#include <QQueue>
#include <QSerialPort>
class Qproduct:public QSerialPort
{
    Q_OBJECT

public:
    explicit Qproduct(QSerialPort *parent = nullptr);
    QSerialPort *serialPort;

};
#endif // QPRODUCT_H
