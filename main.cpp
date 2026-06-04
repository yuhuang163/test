
// #include <DbgHelp.h>  // 包含 Windows 头文件后再引入
// #include <Windows.h>  // 必须放在最前

#include <stdio.h>

#include <QApplication>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextCodec>
#include <QTextStream>
#include <QtDebug>
#include <unordered_map>

#include "PressCalibBox.h"
#include "ageingbox.h"
#include "camerabox.h"
#include "imubox.h"
#include "key_test_box.h"
#include "mainwindow.h"
#include "motorbox.h"
#include "pcbabox.h"
#include "qfreeworkbox.h"
#include "quiescent_current_box.h"
#include "screenbox.h"
#include "suction_box.h"
#include "wifibox.h"
#include "factory_analyzer.h"
#include "common_utils.h"
// #include <Windows.h>

#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
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

// 自定义消息处理函数
void customMessageHandler(QtMsgType type, const QMessageLogContext& context, const QString& msg) {
    QString text;
    switch (type) {
        case QtInfoMsg: text = QString("[Info]"); break;
        case QtDebugMsg: text = QString("[Debug]"); break;
        case QtWarningMsg: text = QString("[Warning]"); break;
        case QtCriticalMsg: text = QString("[Critical]"); break;
        case QtFatalMsg: text = QString("[Fatal]");
    }

    QDateTime current_date_time = QDateTime::currentDateTime();
    QString current_date = current_date_time.toString("yyyy-MM-dd hh:mm:ss.zzz ddd");
    QString message = text.append(current_date)
                          .append(" ")
                          //.append(" file:")
                          .append(QString(context.file).split("\\").last())
                          //.append("function:").append(context.function)
                          // .append(" category:").append(context.category)
                          .append(" line:")
                          .append(QString::number(context.line).append(" 日志内容：" + msg));
    // .append(" version:").append(QString::number(context.version)));

    const QString folderName = QStringLiteral("所有log/上位机log");
    if (!CommonUtils::ensureLogDirectory(folderName)) {
        qDebug() << "无法创建目录:" << folderName;
        return;
    }
    QDir dir(folderName);
    QString fileNumber;
    QRegularExpression re("^\\d+");                           // ^ 表示匹配消息的开始
    QRegularExpressionMatch match = re.match(msg.trimmed());  // 使用 trimmed() 去除字符串开头和结尾的空格
    if (match.hasMatch()) {
        fileNumber = match.captured(0);
    } else {
        // 如果没有找到数字，使用默认值
        fileNumber = "default";
    }
    // 生成文件路径
    QString hostName = QSysInfo::machineHostName();

    const QString fileName = hostName + QStringLiteral("_上位机日志_") + fileNumber + QLatin1Char('_') +
                             CommonUtils::formatDateIso() + QStringLiteral(".txt");
    const QString filePath = CommonUtils::joinPath(folderName, fileName);

    QFile file(filePath);
    const bool isNewLogFile = !file.exists() || file.size() == 0;
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QTextStream text_stream(&file);
    text_stream.setCodec("UTF-8");
    // 新建日志写入 UTF-8 BOM，避免中文 Windows 上被误识别为 GBK/ANSI。
    text_stream.setGenerateByteOrderMark(isNewLogFile);

    printf("%s", current_date.toLocal8Bit().constData());
    // 打印 msg 并换行
    printf("  %s\r\n", msg.toLocal8Bit().constData());
    fflush(stdout);
    text_stream << message << "\r\n";
    file.close();
}
#include <DbgHelp.h>
#include <Windows.h>

#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
// 处理应用程序崩溃时的函数
LONG ApplicationCrashHandler(EXCEPTION_POINTERS* pException) {
    // 获取当前时间
    time_t now = time(0);
    tm* ltm = localtime(&now);

    // 使用 stringstream 格式化时间为 YYYY-MM-DD_HH-MM-SS
    std::stringstream ss;
    ss << std::setw(4) << std::setfill('0') << (1900 + ltm->tm_year) << "-" << std::setw(2) << std::setfill('0')
       << (1 + ltm->tm_mon) << "-" << std::setw(2) << std::setfill('0') << ltm->tm_mday << "_" << std::setw(2)
       << std::setfill('0') << ltm->tm_hour << "-" << std::setw(2) << std::setfill('0') << ltm->tm_min << "-"
       << std::setw(2) << std::setfill('0') << ltm->tm_sec;

    // 获取时间戳字符串
    std::string timeStr = ss.str();

    // 目标文件夹路径
    std::wstring folderPath = L"所有log";

    // 确保文件夹存在
    CreateDirectory(folderPath.c_str(), NULL);

    // 生成 dump 文件的完整路径（添加时间戳）
    std::wstring dumpFilePath = folderPath + L"\\" + std::wstring(timeStr.begin(), timeStr.end()) + L"_闪退记录.dmp";

    // 创建 dump 文件
    HANDLE hDumpFile =
        CreateFile(dumpFilePath.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (hDumpFile != INVALID_HANDLE_VALUE) {
        // Dump信息
        MINIDUMP_EXCEPTION_INFORMATION dumpInfo;
        dumpInfo.ExceptionPointers = pException;
        dumpInfo.ThreadId = GetCurrentThreadId();
        dumpInfo.ClientPointers = TRUE;

        // 写入Dump文件内容
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpNormal, &dumpInfo, NULL, NULL);
        CloseHandle(hDumpFile);
    }

    // 记录堆栈跟踪信息
    HANDLE hProcess = GetCurrentProcess();
    HANDLE hThread = GetCurrentThread();
    CONTEXT context;
    RtlCaptureContext(&context);

    STACKFRAME64 stackFrame;
    memset(&stackFrame, 0, sizeof(STACKFRAME64));

    stackFrame.AddrPC.Mode = AddrModeFlat;
    stackFrame.AddrStack.Mode = AddrModeFlat;
    stackFrame.AddrFrame.Mode = AddrModeFlat;

// Initialize stack frame based on context
#ifdef _M_IX86
    stackFrame.AddrPC.Offset = context.Eip;
    stackFrame.AddrStack.Offset = context.Esp;
    stackFrame.AddrFrame.Offset = context.Ebp;
#elif defined(_M_X64)
    stackFrame.AddrPC.Offset = context.Rip;
    stackFrame.AddrStack.Offset = context.Rsp;
    stackFrame.AddrFrame.Offset = context.Rbp;
#endif

    // 打开DbgHelp库以获取符号信息
    SymInitialize(hProcess, NULL, TRUE);

    // 打印堆栈跟踪
    std::wstring stackTrace = L"堆栈跟踪:\n";
    while (StackWalk64(IMAGE_FILE_MACHINE_AMD64, hProcess, hThread, &stackFrame, &context, NULL,
                       SymFunctionTableAccess64, SymGetModuleBase64, NULL)) {
        DWORD64 dwDisplacement = 0;
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_PATH];
        SYMBOL_INFO* symbol = (SYMBOL_INFO*)symbolBuffer;
        symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbol->MaxNameLen = MAX_PATH;

        if (SymFromAddr(hProcess, stackFrame.AddrPC.Offset, &dwDisplacement, symbol)) {
            stackTrace += std::wstring(symbol->Name, symbol->Name + strlen(symbol->Name)) + L"\n";
        }
    }

    // 记录堆栈信息到 dump 文件中
    std::wstring logFilePath = folderPath + L"\\" + std::wstring(timeStr.begin(), timeStr.end()) + L"_闪退堆栈.log";
    std::ofstream logFile(logFilePath, std::ios::out | std::ios::trunc);

    if (logFile.is_open()) {
        logFile << "崩溃发生时的堆栈跟踪:\n";

        // 将宽字符串 stackTrace 转换为窄字符串
        logFile << std::string(stackTrace.begin(), stackTrace.end());

        logFile.close();
    }
    // 打印堆栈跟踪到控制台
    std::wcout << stackTrace;

    // 弹出消息框
    MessageBoxW(NULL, L"上位机奔溃\r\n奔溃记录已保存", L"Error", MB_OK | MB_ICONERROR);

    return EXCEPTION_EXECUTE_HANDLER;
}

class MyApplication : public QApplication {
public:
    using QApplication::QApplication;

    bool notify(QObject* receiver, QEvent* event) override {
        __try {
            return QApplication::notify(receiver, event);
        } __except (ApplicationCrashHandler(GetExceptionInformation())) {
            // 清空事件队列

            QCoreApplication::processEvents(QEventLoop::AllEvents);
            QCoreApplication::exit(-1);

            return false;
        }
    }
};

int main(int argc, char* argv[]) {
    SetUnhandledExceptionFilter((LPTOP_LEVEL_EXCEPTION_FILTER)ApplicationCrashHandler);  //注冊异常捕获函数

    // QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    // QLoggingCategory::setFilterRules("*.debug=true");


    // 设置使用 UTF-8 编码
    // QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    MyApplication a(argc, argv);

    // 安装自定义消息处理程序
    qInstallMessageHandler(customMessageHandler);

    // qDebug() << "串口问题"<<QSslSocket::sslLibraryBuildVersionString();
    a.setFont(QFont("Microsoft Yahei", 9));
    qRegisterMetaType<FacErrorCode>("FacErrorCode");
    qRegisterMetaType<ProtocolSnData>("ProtocolSnData");
    qRegisterMetaType<ProtocolBatteryData>("ProtocolBatteryData");
    qRegisterMetaType<ProtocolWifiStateData>("ProtocolWifiStateData");
    qRegisterMetaType<ProtocolMusicStateData>("ProtocolMusicStateData");
    qRegisterMetaType<ProtocolBaseInfoData>("ProtocolBaseInfoData");
    qRegisterMetaType<ProtocolPeriphStateData>("ProtocolPeriphStateData");
    qRegisterMetaType<ProtocolButtonStateData>("ProtocolButtonStateData");
    qRegisterMetaType<ProtocolBrushControlData>("ProtocolBrushControlData");
    qRegisterMetaType<ProtocolLedControlData>("ProtocolLedControlData");
    qRegisterMetaType<ProtocolLcdControlData>("ProtocolLcdControlData");
    qRegisterMetaType<ProtocolPressSampleData>("ProtocolPressSampleData");
    qRegisterMetaType<ProtocolImuSampleData>("ProtocolImuSampleData");
    qRegisterMetaType<ProtocolImuCalibResultData>("ProtocolImuCalibResultData");
    qRegisterMetaType<ProtocolPressCalibResultData>("ProtocolPressCalibResultData");
    qRegisterMetaType<ProtocolInternetOtaData>("ProtocolInternetOtaData");
    qRegisterMetaType<ProtocolWifiDemandData>("ProtocolWifiDemandData");
    qRegisterMetaType<ProtocolCameraControlData>("ProtocolCameraControlData");
    qRegisterMetaType<ProtocolServoMotorInfoData>("ProtocolServoMotorInfoData");
    qRegisterMetaType<ProtocolPictureSendOverData>("ProtocolPictureSendOverData");
    qRegisterMetaType<ProtocolPhotosensitiveData>("ProtocolPhotosensitiveData");
    qRegisterMetaType<ProtocolSdInfoData>("ProtocolSdInfoData");

    std::unordered_map<QString, int> map = {{"QUIESCENT_CURRENT", 1}, {"MOTOR_TEST", 2},  {"IMU_CALI", 3},
                                            {"SCREEN_TEST", 4},       {"CAMERA_TEST", 5}, {"WIFIBLE_TEST", 6},
                                            {"AGE_TEST", 7},          {"PCBA_TEST", 8},   {"PRESS_TEST", 9},
                                            {"FREE_WORK", 10},        {"MAIN_TEST", 11},  {"dji_TEST", 12},
                                            {"KEY_TEST", 13},         {"SUCTION_TEST", 14}};

    int exitCode = 0;
    do {
        a.setProperty("StationRestartRequested", false);
        QString station = SETTINGS.value("SYSTEM/station").toString();  // 工站
        qDebug() << "工站为：" + station;

        switch (map[station]) {
            case 1: {
                quiescent_current_box* w = new quiescent_current_box;  // 静态电流
                w->show();
                w->TotallyTask();
                delete w;
                break;
            }

            case 2: {
                motorbox* m = new motorbox;  // 电机测试
                m->show();
                m->TotallyTask();
                delete m;
                break;
            }

            case 3: {
                imubox* i = new imubox;  // imu校准
                i->show();
                i->TotallyTask();
                delete i;
                break;
            }

            case 4: {
                screenbox* c = new screenbox;  // 屏幕测试
                c->show();
                c->TotallyTask();
                delete c;
                break;
            }

            case 5: {
                camerabox* c = new camerabox;  // 摄像头测试
                c->show();
                c->TotallyTask();
                delete c;
                break;
            }

            case 6: {
                wifibox* f = new wifibox;  // wifi蓝牙测试
                f->show();
                f->TotallyTask();
                delete f;
                break;
            }

            case 7: {
                ageingbox* x = new ageingbox;  // 老化测试工站
                x->show();
                x->TotallyTask();
                delete x;
                break;
            }
            case 8: {
                pcbabox* p = new pcbabox;  // 老化测试工站
                p->show();
                p->TotallyTask();
                delete p;
                break;
            }
            case 9: {
                PressCalibBox* c = new PressCalibBox;  // 压感校准测试工站
                c->show();
                c->TotallyTask();
                delete c;
                break;
            }
            case 10: {
                QFreeWorkBox* f = new QFreeWorkBox;  // 自由工站
                f->show();
                f->TotallyTask();
                delete f;
                break;
            }
            case 11: {
                MainWindow h;  // 主测试
                h.show();
                exitCode = a.exec();
                break;
            }
            case 12: {
                factory_analyzer dji;  // 主测试
                dji.show();
                exitCode = a.exec();
                break;
            }
            case 13: {
                key_test_box* k = new key_test_box;  // 按键测试
                k->show();
                k->TotallyTask();
                delete k;
                break;
            }
            case 14: {
                suction_box* s = new suction_box;  // 吸力测试
                s->show();
                s->TotallyTask();
                delete s;
                break;
            }
            default:
                factory_analyzer dji;  // 主测试
                dji.show();
                exitCode = a.exec();
                break;
        }
    } while (a.property("StationRestartRequested").toBool());

    return exitCode;
}
