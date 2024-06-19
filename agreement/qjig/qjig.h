#ifndef QJIG_H
#define QJIG_H
#include <QMessageBox>
#include <QObject>
#include <QQueue>
#include <QSerialPort>
#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
class Qjig : public QSerialPort
{
    Q_OBJECT
public:
    explicit Qjig(QSerialPort *parent = nullptr);
    QSerialPort *serialPort;
    typedef enum
    {
        STATE_CYLINDER_OPEN,   // 开启气缸1
        STATE_RELAY_OPEN,
        STATE_RELAY_RESET,
        STATE_RELAY1_OPEN,      // 继电器1吸合
        STATE_RELAY1_RESET,     // 继电器1复位
        STATE_RELAY2_OPEN,      // 继电器1吸合
        STATE_RELAY2_RESET,     // 继电器1复位
        STATE_CYLINDER_RESET,   // 气缸1复位
        STATE_RELAY4_OPEN,      // 继电器4吸合
        STATE_RELAY4_RESET,     // 继电器4复位
        STATE_RESET_ALL,
        STATE_PEN_PRESS,
    } jigState;
    void sendjigData(jigState fixstate);
    void set_cylinder_state(int state);
    void waitWork(int ms);
    void set_relay_state(int state);
};

#endif   // QJIG_H
