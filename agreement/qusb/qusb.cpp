#include "qusb.h"
#include <QDebug>
#include <cmath>
#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
Qusb::Qusb(QSerialPort *parent) :
QSerialPort(parent), serialPort(parent)
{


        if (serialPort) {
        qDebug() << "Qusb指向" << serialPort;
    } else {
        qDebug() << "Qusb指向一个空指针";
    }

    registerCommand();
}

void Qusb::registerCommand()
{

    // 在这里注册命令即可
    commandList["CURR"] = std::bind(&Qusb::CONFigureFUNCtion, this, std::placeholders::_1);

}
void Qusb::CONFigureFUNCtion(QString p)
{
    qDebug() << "当前是电流模式";
}

void Qusb::parseCmd(const QByteArray &byte)
{
    processModbusRTUData(byte);


    foreach(char c, byte)
    {
        if (c == '\r' || c == '\n')
        {
            // 遇到\r或\n时停止追加字符
            break;
        }
        dataQueue.push_back((uint8_t)c);
    }
    while (dataQueue.isEmpty() == false)
    {
        char c = dataQueue.dequeue();
        cmd += c;
    }
    processCmd(cmd, parameter);
    cmd.clear();
}

void Qusb::processCmd(QString cmd, QString parameter)
{
    auto it = commandList.find(cmd);
    if (it != commandList.end())
    {
        // 调用回调函数
        it->second(parameter);
    }
    else
    {
        emit send_ammeter_data(cmd);   // 发给欣旺达静态电流处理
        qDebug() << "不支持的命令 : " << cmd;
    }
}
void Qusb::sendCmd(QString cmd)
{



    if (!serialPort->isOpen())
    {
        QMessageBox::warning(NULL, "警告", " 未打开usb串口\t\r\n  无法发送数据！\r\n");
        return;
    }

    cmd += "\r\n";

    serialPort->write(cmd.toLocal8Bit());
}
void Qusb::sendCONF(QString current, QString dc, QString range)
{
    // 构建 CONF 命令字符串
    QString command = "CONF:" + current + ":" + dc + " " + range;

    // 调用 sendCmd 函数发送命令
    sendCmd(command);
}

void Qusb::getCONF(QString mac)
{
    QString s;
    // s = "CONFigure:RANGe?";
    // sendCmd(s);
    s = "CONFigure:FUNCtion?";
    sendCmd(s);
}
void Qusb::getMEASure(QString mac)
{
    QString s = "MEASure:CURRent:DC? 500e-3";
    sendCmd(s);
}
void Qusb::gethqMEASure()
{
    QString s = "01030000000305CB";   // 测量数值

    QByteArray data = QByteArray::fromHex(s.toLatin1());
    serialPort->write(data);
}
void Qusb::getlxMEASure(int mechine)
{
    QString s;
    if (mechine == 1)
        s = "010300000002c40b";   // 测量数值
    if (mechine == 2)
        s = "020300000002C438";   // 测量数值
    if (mechine == 3)
        s = "030300000002C5E9";   // 测量数值
    if (mechine == 4)
        s = "040300000002C45E";   // 测量数值
    if (mechine == 5)
        s = "050300000002C58F";   // 测量数值
    if (mechine == 6)
        s = "060300000002C5BC";   // 测量数值

    QByteArray data = QByteArray::fromHex(s.toLatin1());
    serialPort->write(data);
}
void Qusb::sethqMEASure()
{
    QString s = "010600030006F9C8";   // 设置波特率115200
    QByteArray data = QByteArray::fromHex(s.toLatin1());
    serialPort->write(data);
}
quint16 calculateCRC(const QByteArray &data)
{
    quint16 crc = 0xFFFF;

    for (int i = 0; i < data.length(); ++i)
    {
        crc ^= static_cast<quint8>(data.at(i));
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 0x0001)
            {
                crc = (crc >> 1) ^ 0xA001;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    // Correct the endianness
    return ((crc & 0x00FF) << 8) | ((crc & 0xFF00) >> 8);
}
void Qusb::processModbusRTUData(const QByteArray &response)
{
    // Modbus RTU响应帧的最小长度为 5 字节
    if (response.length() >= 5)
    {
        qDebug() << "Received Modbus RTU response frame: " << response.toHex().toUpper();

        // 解析响应帧

        quint8 byteCount = static_cast<quint8>(response.at(2));

        // 验证数据长度是否符合预期
        if (response.length() == 5 + byteCount)
        {
            // 验证CRC校验
            quint16 crcExpected = (static_cast<quint8>(response.at(response.length() - 2)) << 8) |
                                  static_cast<quint8>(response.at(response.length() - 1));
            quint16 crcCalculated = calculateCRC(response.left(response.length() - 2));

            qDebug() << "Length: " << response.length() << " Byte Count: " << byteCount
                     << " Expected CRC: " << QString::number(crcExpected, 16).toUpper()
                     << " Calculated CRC: " << QString::number(crcCalculated, 16).toUpper();

            if (crcCalculated == crcExpected)
            {
                quint16 reg0 = (static_cast<quint8>(response.at(3)) << 8) |
                               static_cast<quint8>(response.at(4));
                quint16 reg1 = (static_cast<quint8>(response.at(5)) << 8) |
                               static_cast<quint8>(response.at(6));

                // 读取电流单位指数
                qint8 unitExponent = static_cast<qint8>(response.at(8));

                // 计算电流值
                qint32 currentValue = (reg0 << 16) | reg1;

                // 根据单位指数调整电流值
                double current_uA = currentValue * std::pow(10, unitExponent) * 0.000000001;
                qDebug() << ": Current Value = " << current_uA << "A";

                QString stringValue = QString::number(current_uA);
                emit send_ammeter_data(stringValue);
            }
            else
            {
                qDebug() << "CRC check failed";
            }
        }
        else
        {
            qDebug() << "Invalid data length";
        }
    }
    else
    {
        qDebug() << "Invalid Modbus RTU response frame";
    }
}

void Qusb::processlxModbusRTUData(const QByteArray &response)
{
    qDebug() << "收到的数据为: " << response.toHex().toUpper();
    data = data + response;
    if (data.length() > 15)
        data = 0;

    if (data.length() >= 5)
    {
        qDebug() << "Received Modbus RTU data frame: " << data.toHex().toUpper();
        quint8 byteCount = static_cast<quint8>(data.at(2));
        qDebug() << "数据长度：" << byteCount;
        // 验证数据长度是否符合预期
        if (data.length() == 5 + byteCount)
        {
            // 验证CRC校验
            quint16 crcExpected = (static_cast<quint8>(data.at(data.length() - 2)) << 8) |
                                  static_cast<quint8>(data.at(data.length() - 1));
            quint16 crcCalculated = calculateCRC(data.left(data.length() - 2));

            qDebug() << "Length: " << data.length()
                     << " Expected CRC: " << QString::number(crcExpected, 16).toUpper()
                     << " Calculated CRC: " << QString::number(crcCalculated, 16).toUpper();

            if (crcCalculated == crcExpected)
            {
                quint16 reg0 =
                    (static_cast<quint8>(data.at(3)) << 8) | static_cast<quint8>(data.at(4));
                quint16 reg1 =
                    (static_cast<quint8>(data.at(5)) << 8) | static_cast<quint8>(data.at(6));

                // 读取电流单位指数
                // qint8 unitExponent = static_cast<qint8>(response.at(8));

                // 计算电流值
                qint32 currentValue = (reg0 << 16) | reg1;

                // 根据单位指数调整电流值
                double current_uA = currentValue;
                qDebug() << "Current Value = " << current_uA << "uA";

                QString stringValue = QString::number(current_uA);
                emit send_ammeter_data(stringValue);
                data = 0;
            }
            else
            {
                qDebug() << "CRC check failed";
            }
        }
        else
        {
            qDebug() << "Invalid data length";
        }
    }
    else
    {
        qDebug() << "Invalid Modbus RTU response frame";
    }
}
