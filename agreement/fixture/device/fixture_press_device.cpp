#include "fixture_press_device.h"

#include <cstdlib>

#include <QDebug>
#include <QString>

#include "Abini.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

void FixturePressUartProtocol::initCommands() {
#if 1
    press_commands_[COMMAND_ID_TRAY_IN][0] = {"TARY_I\r\n"};
    press_commands_[COMMAND_ID_TRAY_OUT][0] = {"TARY_O\r\n"};
    press_commands_[COMMAND_ID_FIXED_BLOCK_UP][0] = {"PROD_U\r\n"};
    press_commands_[COMMAND_ID_FIXED_BLOCK_DOWN][0] = {"PROD_D\r\n"};
    press_commands_[COMMAND_ID_KEY_UP][0] = {"FAMA_U\r\n"};
    press_commands_[COMMAND_ID_KEY_DOWN_200][0] = {"FAMA_D\r\n"};
    press_commands_[COMMAND_ID_BTH_UP][0] = {"HEAD_U\r\n"};
    press_commands_[COMMAND_ID_BTH_DOWN_200][0] = {"HEAD_D\r\n"};
#else
    press_commands_[COMMAND_ID_TRAY_IN][0] = {"TARY_I"};
    press_commands_[COMMAND_ID_TRAY_OUT][0] = {"TARY_O"};
    press_commands_[COMMAND_ID_FIXED_BLOCK_UP][0] = {"PROD_U"};
    press_commands_[COMMAND_ID_FIXED_BLOCK_DOWN][0] = {"PROD_D"};
    press_commands_[COMMAND_ID_KEY_UP][0] = {"FAMA_U"};
    press_commands_[COMMAND_ID_KEY_DOWN_200][0] = {"FAMA_D"};
    press_commands_[COMMAND_ID_BTH_UP][0] = {"HEAD_U"};
    press_commands_[COMMAND_ID_BTH_DOWN_200][0] = {"HEAD_D"};
#endif

    press_commands_[COMMAND_ID_CHEAK_STATUS][0] = {"Check:Pass\r"};
    press_commands_[COMMAND_ID_RESULT][0] = {"End:1,1,1\r"};
    press_commands_[COMMAND_ID_RESULT][1] = {"End:1,1,1,1\r"};
    for (int i = 0; i < 4; i++) {
        const QString command1 = QString("Set:%1H,500\r").arg(i + 1);
        const QString command2 = QString("Set:%1H:3500\r").arg(i + 1);
        const QString command3 = QString("Set:%1H:4500\r").arg(i + 1);
        const QString command4 = QString("Rst:%1\r").arg(i + 1);
        const QString command5 = QString("Set:%1B:2300\r").arg(i + 1);
        const QString command6 = QString("Rst:%1\r").arg(i + 1);

        press_commands_[COMMAND_ID_BTH_PRESS_50][i] = strdup(command1.toStdString().c_str());
        press_commands_[COMMAND_ID_BTH_PRESS_350][i] = strdup(command2.toStdString().c_str());
        press_commands_[COMMAND_ID_BTH_PRESS_450][i] = strdup(command3.toStdString().c_str());
        press_commands_[COMMAND_ID_BTH_PRESS_UP][i] = strdup(command4.toStdString().c_str());
        press_commands_[COMMAND_ID_KEY_PRESS_230][i] = strdup(command5.toStdString().c_str());
        press_commands_[COMMAND_ID_KEY_PRESS_UP][i] = strdup(command6.toStdString().c_str());

        qDebug() << "commmmmmmm:" << press_commands_[COMMAND_ID_BTH_PRESS_50][i];
    }

    press_commands_[COMMAND_ID_FAMA_100_O][0] = {"FAMA1_100G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_100_C][0] = {"FAMA1_100G_C\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_O][0] = {"FAMA1_300G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_C][0] = {"FAMA1_300G_C\r\n"};

    press_commands_[COMMAND_ID_FAMA_100_O][1] = {"FAMA2_100G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_100_C][1] = {"FAMA2_100G_C\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_O][1] = {"FAMA2_300G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_C][1] = {"FAMA2_300G_C\r\n"};

    press_commands_[COMMAND_ID_FAMA_100_O][2] = {"FAMA3_100G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_100_C][2] = {"FAMA3_100G_C\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_O][2] = {"FAMA3_300G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_C][2] = {"FAMA3_300G_C\r\n"};

    press_commands_[COMMAND_ID_FAMA_100_O][3] = {"FAMA4_100G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_100_C][3] = {"FAMA4_100G_C\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_O][3] = {"FAMA4_300G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_C][3] = {"FAMA4_300G_C\r\n"};

    press_commands_[COMMAND_ID_FAMA_100_O][4] = {"FAMA5_100G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_100_C][4] = {"FAMA5_100G_C\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_O][4] = {"FAMA5_300G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_C][4] = {"FAMA5_300G_C\r\n"};

    press_commands_[COMMAND_ID_FAMA_100_O][5] = {"FAMA6_100G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_100_C][5] = {"FAMA6_100G_C\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_O][5] = {"FAMA6_300G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_300_C][5] = {"FAMA6_300G_C\r\n"};

    press_commands_[COMMAND_ID_FAMA_UP][0] = {"UP\r\n"};
    press_commands_[COMMAND_ID_FAMA_DOWN][0] = {"DOWN\r\n"};

    press_commands_[COMMAND_ID_FAMA_400_O][0] = {"FAMA1_400G_C\r\n"};
    press_commands_[COMMAND_ID_FAMA_400_C][0] = {"FAMA1_400G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_50_O][0] = {"FAMA1_50G_C\r\n"};
    press_commands_[COMMAND_ID_FAMA_50_C][0] = {"FAMA1_50G_O\r\n"};
    press_commands_[COMMAND_ID_FAMA_200_O][0] = {"FAMA1_200G_C\r\n"};
    press_commands_[COMMAND_ID_FAMA_200_C][0] = {"FAMA1_200G_O\r\n"};
}

bool FixturePressUartProtocol::isValidCommand(int commandId, int numb) const {
    return commandId > COMMAND_ID_INVALID && commandId < COMMAND_ID_MAX && numb >= 0 && numb < 6 &&
        press_commands_[commandId][numb] != nullptr;
}

bool FixturePressUartProtocol::commandBytes(int commandId, int numb, QByteArray* out) const {
    if (!out || !isValidCommand(commandId, numb)) {
        return false;
    }
    *out = QByteArray(press_commands_[commandId][numb]);
    return true;
}

QByteArray FixturePressUartProtocol::buildFixtureStateCommand(FixtureState state) {
    switch (state) {
    case STATE_CYLINDER_OPEN:
        return QByteArray::fromHex("5501050001");
    case STATE_RELAY1_OPEN:
        return QByteArray::fromHex("5501010001");
    case STATE_RELAY1_RESET:
        return QByteArray::fromHex("5501010000");
    case STATE_CYLINDER_RESET:
        return QByteArray::fromHex("5501050000");
    case STATE_RELAY4_OPEN:
        return QByteArray::fromHex("5501040001");
    case STATE_RELAY4_RESET:
        return QByteArray::fromHex("5501040000");
    default:
        return QByteArray();
    }
}

FixturePressUartEvent FixturePressUartProtocol::parseReceived(const QByteArray& data) {
    FixturePressUartEvent ev;
    const QString text = QString::fromUtf8(data);

    if (text.contains(QStringLiteral("BarCode?\r"))) {
#if Y20_Pro_PRESS_TEST
        ev.requestCheckStatus = true;
#endif
        ev.pressNotifyState = 1;
    }
    if (text.contains(QStringLiteral("Ready\r"))) {
        ev.pressNotifyState = 2;
    }
    if (text.contains(QStringLiteral("START1_OK\r\n"))) {
        ev.pressNotifyState = 3;
    }
    if (text.contains(QStringLiteral("START2_OK\r\n"))) {
        ev.pressNotifyState = 4;
    }
    return ev;
}

#if _MSC_VER >= 1600
#pragma execution_character_set(pop)
#endif
