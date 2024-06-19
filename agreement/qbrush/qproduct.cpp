#include "qproduct.h"

Qproduct::Qproduct(QSerialPort *parent) : QSerialPort{parent}
{
    serialPort = parent;
}
