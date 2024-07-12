#include "qjig.h"
#include "AbIni.h"
#include "qcoreapplication.h"
#include "qdatetime.h"
#include "qdebug.h"
#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif

Qjig::Qjig(QSerialPort *parent) : QSerialPort(parent), serialPort(parent) {}

void Qjig::set_cylinder_state(int state,int mechine)
{
    // if (getIndex() == 2)
    //     return;
    if (state)
    {
        qDebug() << "打开气缸";

#ifdef NEW_XWD_QUIESCENT_CURRENT

#else
        waitWork(2000);
        sendjigData(STATE_CYLINDER_OPEN);

        waitWork(1500);
#endif

          if (mechine == 1)
                {
        #ifdef NEW_XWD_QUIESCENT_CURRENT

        #else
                    set_relay_state(1);
        #endif
                }
        if (mechine == 2)
                {
        #ifdef NEW_XWD_QUIESCENT_CURRENT
                    sendjigData(STATE_RELAY_OPEN);
                    waitWork(50);
                    sendjigData(STATE_CYLINDER_OPEN);
                    waitWork(3000);
                    sendjigData(STATE_RELAY_RESET);
        #else
                    set_relay_state(2);
        #endif
               }
    }
    else
    {
        // waitWork(5000);
        sendjigData(STATE_CYLINDER_RESET);
        sendjigData(STATE_RESET_ALL);
        // waitWork(5000);
    }
}

void Qjig::waitWork(int ms)
{
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}

void Qjig::set_relay_state(int state)
{
    if (state == 1)
    {
        sendjigData(STATE_RELAY1_OPEN);
        waitWork(200);
        sendjigData(STATE_RELAY1_RESET);
    }
    if (state == 2)
    {
        sendjigData(STATE_RELAY2_OPEN);
        waitWork(200);
        sendjigData(STATE_RELAY2_RESET);
    }
    if (state == 4)
    {
        sendjigData(STATE_RELAY4_OPEN);
        waitWork(200);
        sendjigData(STATE_RELAY4_RESET);
    }
}
void Qjig::sendjigData(jigState fixstate)
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    if (settings.value("Mes/FACTORY", "xwd").toString() == "xwd")
    {
        if (!serialPort->isOpen())
        {
            QMessageBox::warning(NULL, "警告", " 未打开治具串口\t\r\n  无法发送数据！\r\n");
            return;
        }

        switch (fixstate)
        {
        case STATE_CYLINDER_OPEN:
            // serialPort->write(QByteArray::fromHex("5501050001")); // 气缸1动作
            serialPort->write(QByteArray("ctl_down\r\n"));
            // serialPort->write(QByteArray("ctl_down\r\n"));
            break;

        case STATE_CYLINDER_RESET:

#ifdef NEW_XWD_QUIESCENT_CURRENT
            serialPort->write(QByteArray("ctl_up\r\n"));   // 治具上升
#else
            serialPort->write(QByteArray::fromHex("5501050000"));   // 气缸1复位
#endif

            break;
        case STATE_RELAY1_OPEN:
#ifdef NEW_XWD_QUIESCENT_CURRENT
            serialPort->write(QByteArray("pen1_down\r\n"));   // 笔形气缸1按下
#else
            serialPort->write(QByteArray::fromHex("5501010001"));   // 继电器1吸合
#endif
            break;

        case STATE_RELAY_RESET:
#ifdef NEW_XWD_QUIESCENT_CURRENT
            serialPort->write(QByteArray("pen_up\r\n"));   // 笔形气缸1弹起
#else
            serialPort->write(QByteArray::fromHex("5501010000"));   // 继电器1复位
#endif
            break;
        case STATE_RELAY_OPEN:
#ifdef NEW_XWD_QUIESCENT_CURRENT
            serialPort->write(QByteArray("pen_down\r\n"));   // 笔形气缸2按下
#else
            serialPort->write(QByteArray::fromHex("5501020001"));   // 继电器2吸合
#endif
            break;

        case STATE_RELAY1_RESET:
#ifdef NEW_XWD_QUIESCENT_CURRENT
            serialPort->write(QByteArray("pen1_up\r\n"));   // 笔形气缸1弹起
#else
            serialPort->write(QByteArray::fromHex("5501010000"));   // 继电器1复位
#endif
            break;
        case STATE_RELAY2_OPEN:
#ifdef NEW_XWD_QUIESCENT_CURRENT
            serialPort->write(QByteArray("pen2_down\r\n"));   // 笔形气缸2按下
#else
            serialPort->write(QByteArray::fromHex("5501020001"));   // 继电器2吸合
#endif
            break;

        case STATE_RELAY2_RESET:
#ifdef NEW_XWD_QUIESCENT_CURRENT
            serialPort->write(QByteArray("pen2_up\r\n"));   // 笔形气缸2弹起
#else
            serialPort->write(QByteArray::fromHex("5501020000"));   // 继电器2复位
#endif

            break;

        case STATE_RELAY4_OPEN:
            serialPort->write(QByteArray::fromHex("5501040001"));   // 继电器4吸合
            break;
        case STATE_RELAY4_RESET:
            serialPort->write(QByteArray::fromHex("5501040000"));   // 继电器4复位
        case STATE_PEN_PRESS:
            serialPort->write(QByteArray("pen_press\r\n"));   // 笔形气缸同时按下500ms并弹起复位
            break;
        case STATE_RESET_ALL:
            serialPort->write(QByteArray("reset\r\n"));   // 气缸退出，状态复位
            break;
        }
    }
}
