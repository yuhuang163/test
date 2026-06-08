#include "qjig.h"

#include "AbIni.h"
#include "common_utils.h"
#include "qcoreapplication.h"
#include "qdatetime.h"
#include "qdebug.h"
#include <QStringList>
#include <QVector>
#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

Qjig::Qjig(QSerialPort* parent) : QSerialPort(parent), serialPort(parent) {
}

void Qjig::set(JigCmd cmd, const QVariant& data) {
    switch (cmd) {
    case JigCmd::SendState:
        sendjigData(static_cast<jigState>(data.toInt()));
        return;
    case JigCmd::SetCylinderState: {
        const QVariantMap m = data.toMap();
        set_cylinder_state(m.value(QStringLiteral("state")).toInt(),
                           m.value(QStringLiteral("mechine")).toInt());
        return;
    }
    case JigCmd::SetRelayState:
        set_relay_state(data.toInt());
        return;
    case JigCmd::GetAmplitude:
        get_amplitude();
        return;
    }
}

void Qjig::get(JigCmd cmd, const QVariant& param) {
    Q_UNUSED(param);
    if (cmd == JigCmd::GetAmplitude) {
        get_amplitude();
    }
}

bool Qjig::sendCustomMessage(const QVariantMap& map) {
    const QString action = map.value(QStringLiteral("action")).toString().trimmed().toLower();
    if (action == QStringLiteral("sendstate")) {
        set(JigCmd::SendState, map.value(QStringLiteral("state")));
        return true;
    }
    if (action == QStringLiteral("setcylinderstate")) {
        QVariantMap payload;
        payload.insert(QStringLiteral("state"), map.value(QStringLiteral("state")));
        payload.insert(QStringLiteral("mechine"), map.value(QStringLiteral("mechine")));
        set(JigCmd::SetCylinderState, payload);
        return true;
    }
    if (action == QStringLiteral("setrelaystate")) {
        set(JigCmd::SetRelayState, map.value(QStringLiteral("state")));
        return true;
    }
    if (action == QStringLiteral("getamplitude")) {
        get(JigCmd::GetAmplitude);
        return true;
    }
    return false;
}

// state表示阶段
void Qjig::set_cylinder_state(int state, int mechine) {
    // if (getIndex() == 2)
    //     return;
    if (state) {
        qDebug() << "打开气缸";

        if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
            SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
        } else {
            waitWork(2000);
            sendjigData(STATE_CYLINDER_OPEN);

            waitWork(1500);
        }

        if (mechine == 1) {
            if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
            } else {
                set_relay_state(1);
            }
        }
        if (mechine == 2) {
            if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2)

            {
                if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3) {
                    sendjigData(STATE_TRAY_MIDDLE);
                    waitWork(2000);
                }

                sendjigData(STATE_RELAY_OPEN);
                waitWork(50);
                sendjigData(STATE_CYLINDER_OPEN);

                if (SETTINGS.value("Mes/Product_Name").toString() == "Y20PS" ||
                    SETTINGS.value("Mes/Product_Name").toString() == "Y30" ||
                    SETTINGS.value("Mes/Product_Name").toString() == "Y30S")
                    waitWork(5000);
                else
                    waitWork(3000);
                sendjigData(STATE_RELAY_RESET);
            } else {
                set_relay_state(2);
            }
        }
    } else {
        // waitWork(5000);
        sendjigData(STATE_CYLINDER_RESET);
        sendjigData(STATE_RESET_ALL);

        if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3) {
            waitWork(2000);
            sendjigData(STATE_TRAY_REAR);
        }
    }
}

void Qjig::waitWork(int ms) {
    QTime t;
    t.start();
    while (t.elapsed() < ms)
        QCoreApplication::processEvents();
}

void Qjig::set_relay_state(int state) {
    if (state == 1) {
        sendjigData(STATE_RELAY1_OPEN);
        waitWork(200);
        sendjigData(STATE_RELAY1_RESET);
    }
    if (state == 2) {
        sendjigData(STATE_RELAY2_OPEN);
        waitWork(200);
        sendjigData(STATE_RELAY2_RESET);
    }
    if (state == 4) {
        sendjigData(STATE_RELAY4_OPEN);
        waitWork(200);
        sendjigData(STATE_RELAY4_RESET);
    }
}
void Qjig::sendjigData(jigState fixstate) {
    if (SETTINGS.value("Mes/FACTORY", "xwd").toString() == "xwd") {
        if (!serialPort->isOpen()) {
            QMessageBox::warning(NULL, "警告", " 未打开治具串口\t\r\n  无法发送数据！\r\n");
            return;
        }

        QByteArray dataToSend;

        switch (fixstate) {
        case STATE_CYLINDER_OPEN:

            if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
                dataToSend = QByteArray("ctl_down\r\n");
            } else {
                // dataToSend = QByteArray::fromHex("5501050001");  //不希望夹手
            }

            break;

        case STATE_CYLINDER_RESET:
            if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
                dataToSend = QByteArray("ctl_up\r\n");
            } else {
                dataToSend = QByteArray::fromHex("5501050000");
            }

            break;
        case STATE_RELAY1_OPEN:

            if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
                dataToSend = QByteArray("pen1_down\r\n");
            } else {
                dataToSend = QByteArray::fromHex("5501010001");
            }

            break;

        case STATE_RELAY_RESET:

            if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
                dataToSend = QByteArray("pen_up\r\n");
            } else {
                dataToSend = QByteArray::fromHex("5501010000");
            }

            break;
        case STATE_RELAY_OPEN:
            if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
                dataToSend = QByteArray("pen_down\r\n");
            } else {
                dataToSend = QByteArray::fromHex("5501020001");
            }

            break;

        case STATE_RELAY1_RESET:
            if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
                dataToSend = QByteArray("pen1_up\r\n");
            } else {
                dataToSend = QByteArray::fromHex("5501010000");
            }

            break;
        case STATE_RELAY2_OPEN:

            if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
                dataToSend = QByteArray("pen2_down\r\n");
            } else {
                dataToSend = QByteArray::fromHex("5501020001");
            }

            break;

        case STATE_RELAY2_RESET:

            if (SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 3 ||
                SETTINGS.value("SYSTEM/CurrentMechine").toInt() == 2) {
                dataToSend = QByteArray("pen2_up\r\n");
            } else {
                dataToSend = QByteArray::fromHex("5501020000");
            }

            break;

        case STATE_RELAY4_OPEN:
            dataToSend = QByteArray::fromHex("5501040001"); // 继电器4吸合
            break;
        case STATE_RELAY4_RESET:
            dataToSend = QByteArray::fromHex("5501040000"); // 继电器4复位
            break;
        case STATE_PEN_PRESS:
            dataToSend = QByteArray("pen_press\r\n");
            // serialPort->write(QByteArray("pen_press\r\n"));  // 笔形气缸同时按下500ms并弹起复位
            break;
        case STATE_RESET_ALL:
            dataToSend = QByteArray("reset\r\n");
            // serialPort->write(QByteArray("reset\r\n"));  // 气缸退出，状态复位
            break;
        case STATE_TRAY_MIDDLE: //进去
            dataToSend = QByteArray("tray_middle\r\n");
            // serialPort->write(QByteArray("tray_middle\r\n"));
            break;
        case STATE_TRAY_REAR: //下一个工站
            dataToSend = QByteArray("tray_rear\r\n");
            //  serialPort->write(QByteArray("tray_rear\r\n"));
            break;
        case STATE_FRONT: //出来原位
            dataToSend = QByteArray("tray_front\r\n");
            //  serialPort->write(QByteArray("tray_front\r\n"));
            break;
        }

        if (!dataToSend.isEmpty()) {
            qDebug().noquote() << "JIG TX:" << QString::fromLatin1(dataToSend.toHex(' ').toUpper());
            serialPort->write(dataToSend);
            log_.save_jig_uart_log(1, dataToSend);
        }
    }
}
void Qjig::parseCmd(const QByteArray& byte) {
    if (byte.isEmpty()) {
        return;
    }
    qDebug().noquote() << "JIG RX:" << QString::fromLatin1(byte.toHex(' ').toUpper());
    qDebug() << "byte" << byte;

    QList<int> allValues;   // 存储所有行的数值
    bool foundData = false; // 标记是否找到数据行

    // 将数据按行分割
    QList<QByteArray> lines = byte.split('\n');
    for (const QByteArray& line : lines) {
        if (line.trimmed().isEmpty()) {
            continue;
        }
        qDebug() << "line" << line;

        // 检查是否是数据行
        QByteArray searchPattern = QByteArray::fromHex("CAFDBEDD"); // "数据"的GBK编码
        if (line.contains(searchPattern)) {
            foundData = true;
            // 解析数据行，提取数值
            QByteArray colon = QByteArray::fromHex("A3BA"); // 中文冒号的GBK编码
            int colonPos = line.indexOf(colon);
            if (colonPos != -1) {
                QByteArray valuePart = line.mid(colonPos + colon.length());
                QString dataStr = QString::fromUtf8(valuePart.trimmed());
                bool ok;
                int value = dataStr.toInt(&ok);
                if (ok) {
                    allValues.append(value);
                }
            }
        }
    }

    // 所有行遍历完成后，计算摆幅
    if (foundData && !allValues.isEmpty()) {
        // 找出非零值的最大值和最小值
        int maxVal = 0;
        int minVal = 99999;
        bool hasNonZeroValue = false;

        for (int value : allValues) {
            if (value > 0) {
                hasNonZeroValue = true;
                maxVal = qMax(maxVal, value);
                minVal = qMin(minVal, value);
            }
        }

        if (hasNonZeroValue) {
            // 计算摆幅：最大值减最小值
            int amplitude = maxVal - minVal + 1;
            // 获取误差值
            int errorValue = SETTINGS.value("Pressure/AmplitudeError", 0).toInt();
            // 计算最终结果：摆幅加上误差值后乘以6
            int result = (amplitude + errorValue) * 6;
            qDebug() << "发送结果，摆幅:" << amplitude << "误差值:" << errorValue;
            emitReport(QStringLiteral("ProtocolJigAmplitudeData"),
                       QVariant::fromValue(ProtocolJigAmplitudeData{QString::number(result)}));
        }
    }
}
void Qjig::get_amplitude() {
    QByteArray dataToSend = QByteArray("Test1\r\n");
    if (!dataToSend.isEmpty()) {
        qDebug().noquote() << "JIG TX:" << QString::fromLatin1(dataToSend.toHex(' ').toUpper());
        serialPort->write(dataToSend);
        log_.save_jig_uart_log(1, dataToSend);
    }
}
