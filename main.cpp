
// #include <DbgHelp.h>  // 包含 Windows 头文件后再引入
// #include <Windows.h>  // 必须放在最前

#include <stdio.h>

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QSysInfo>
#include <QMessageBox>
#include <QTextCodec>
#include <QTextStream>
#include <QtDebug>
#include <unordered_map>
#include "qprotocol_types.h"

#include "AbIni.h"
#include "common_utils.h"
#include "qlog.h"
#include "login_dialog.h"
#include "auth_service.h"
#include "test_case_sync_service.h"
#include "key_test_box.h"
#include "qfreeworkbox.h"
#include "quiescent_current_box.h"
#include "suction_box.h"
#include "ageingbox.h"
#include "mainwindow.h"
#include "factory_analyzer.h"

#if ENABLE_STATION_PRESS_TEST
#include "PressCalibBox.h"
#endif
#if ENABLE_STATION_CAMERA_TEST
#include "camerabox.h"
#endif
#if ENABLE_STATION_IMU_CALI
#include "imubox.h"
#endif
#if ENABLE_STATION_MOTOR_TEST
#include "motorbox.h"
#endif
#if ENABLE_STATION_PCBA_TEST
#include "pcbabox.h"
#endif
#if ENABLE_STATION_SCREEN_TEST
#include "screenbox.h"
#endif
#if ENABLE_STATION_WIFIBLE_TEST
#include "wifibox.h"
#endif

#ifdef Q_OS_WIN
#include "qlog_win.h"
#endif

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// 1.引入文件	#include <stdio.h>
// 		stdio 就是指 “standard input & output"（标准输入输出）
// 	所以，源代码中如用到标准输入输出函数时，就要包含这个头文件！
// 	例如c语言中的 printf("%d",i); scanf("%d",&i);等函数。

// 2.使用printf()
// 	eg：	printf("hello QT");

// 3.清空缓冲区
// 	fflush(stdout);
// 	fflush(stdout)刷新标准输出缓冲区，把输出缓冲区里的东西打印到标准输出设备上
// qInfo() << "This is an info message.";
// qDebug() << "This is a debug message.";
// qWarning() << "This is a warning message.";
// qCritical() << "This is a critical message.";
// qFatal("This is a fatal message.");
// drmemory.exe -- new_production_20250228.exe

class MyApplication : public QApplication {
  public:
    using QApplication::QApplication;

    bool notify(QObject* receiver, QEvent* event) override {
        __try {
            return QApplication::notify(receiver, event);
        } __except (QlogApplicationCrashHandler(GetExceptionInformation())) {
            // 清空事件队列

            QCoreApplication::processEvents(QEventLoop::AllEvents);
            QCoreApplication::exit(-1);

            return false;
        }
    }
};

int main(int argc, char* argv[]) {
    Qlog::installWindowsCrashHandler();

    // QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    // QLoggingCategory::setFilterRules("*.debug=true");

    // 设置使用 UTF-8 编码
    // QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    MyApplication a(argc, argv);

    SETTINGS.migrateFactoryCloudToLocal();

    Qlog::setCrashReportExtraInfo(QStringLiteral("station=%1, cwd=%2, host=%3")
                                      .arg(SETTINGS.value(QStringLiteral("SYSTEM/station")).toString(),
                                           QDir::currentPath(), QSysInfo::machineHostName()));

    Qlog::installQtMessageHandler();

    // qDebug() << "串口问题"<<QSslSocket::sslLibraryBuildVersionString();
    a.setFont(QFont("Microsoft Yahei", 9));
    qRegisterMetaType<FacErrorCode>("FacErrorCode");
    qRegisterMetaType<ProtocolReport>("ProtocolReport");
    qRegisterMetaType<ProtocolMeasureData>("ProtocolMeasureData");

    std::unordered_map<QString, int> map = {{"QUIESCENT_CURRENT", 1}, {"MOTOR_TEST", 2}, {"IMU_CALI", 3}, {"SCREEN_TEST", 4}, {"CAMERA_TEST", 5}, {"WIFIBLE_TEST", 6}, {"AGE_TEST", 7}, {"PCBA_TEST", 8}, {"PRESS_TEST", 9}, {"FREE_WORK", 10}, {"MAIN_TEST", 11}, {"dji_TEST", 12}, {"KEY_TEST", 13}, {"SUCTION_TEST", 14}};

    if (!AuthService::isLoggedIn()) {
        AuthService::LoginResult autoResult = AuthService::loginWithSavedCredentials();
        if (!autoResult.ok) {
            LoginDialog loginDialog;
            if (loginDialog.exec() != QDialog::Accepted) {
                return 0;
            }
        }
    }

    // 登录后启动心跳/命令轮询，供网页拉取产线工站用例
    TestCaseSyncService::startDeviceAgent();

    int exitCode = 0;
    do {
        a.setProperty("StationRestartRequested", false);
        QString station = SETTINGS.value("SYSTEM/station").toString(); // 工站
        if (!isStationEnabled(station)) {
            qWarning().noquote() << QStringLiteral("工站已屏蔽，改用自由工站：") << station;
            station = QStringLiteral("FREE_WORK");
            SETTINGS.setValue(QStringLiteral("SYSTEM/station"), station);
        }
        qDebug() << "工站为：" + station;

        switch (map[station]) {
        case 1: {
            quiescent_current_box* w = new quiescent_current_box; // 静态电流
            w->show();
            w->TotallyTask();
            delete w;
            break;
        }

#if ENABLE_STATION_MOTOR_TEST
        case 2: {
            motorbox* m = new motorbox; // 电机测试
            m->show();
            m->TotallyTask();
            delete m;
            break;
        }
#endif

#if ENABLE_STATION_IMU_CALI
        case 3: {
            imubox* i = new imubox; // imu校准
            i->show();
            i->TotallyTask();
            delete i;
            break;
        }
#endif

#if ENABLE_STATION_SCREEN_TEST
        case 4: {
            screenbox* c = new screenbox; // 屏幕测试
            c->show();
            c->TotallyTask();
            delete c;
            break;
        }
#endif

#if ENABLE_STATION_CAMERA_TEST
        case 5: {
            camerabox* c = new camerabox; // 摄像头测试
            c->show();
            c->TotallyTask();
            delete c;
            break;
        }
#endif

#if ENABLE_STATION_WIFIBLE_TEST
        case 6: {
            wifibox* f = new wifibox; // wifi蓝牙测试
            f->show();
            f->TotallyTask();
            delete f;
            break;
        }
#endif

        case 7: {
            ageingbox* x = new ageingbox; // 老化测试工站
            x->show();
            x->TotallyTask();
            delete x;
            break;
        }
#if ENABLE_STATION_PCBA_TEST
        case 8: {
            pcbabox* p = new pcbabox; // 老化测试工站
            p->show();
            p->TotallyTask();
            delete p;
            break;
        }
#endif
#if ENABLE_STATION_PRESS_TEST
        case 9: {
            PressCalibBox* c = new PressCalibBox; // 压感校准测试工站
            c->show();
            c->TotallyTask();
            delete c;
            break;
        }
#endif
        case 10: {
            QFreeWorkBox* f = new QFreeWorkBox; // 自由工站
            f->show();
            f->TotallyTask();
            delete f;
            break;
        }
        case 11: {
            MainWindow h; // 主测试
            h.show();
            exitCode = a.exec();
            break;
        }
        case 12: {
            factory_analyzer dji; // 主测试
            dji.show();
            exitCode = a.exec();
            break;
        }
        case 13: {
            key_test_box* k = new key_test_box; // 按键测试
            k->show();
            k->TotallyTask();
            delete k;
            break;
        }
        case 14: {
            suction_box* s = new suction_box; // 吸力测试
            s->show();
            s->TotallyTask();
            delete s;
            break;
        }
        default: {
            factory_analyzer dji; // 主测试
            dji.show();
            exitCode = a.exec();
            break;
        }
        }
    } while (a.property("StationRestartRequested").toBool());

    return exitCode;
}
