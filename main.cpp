#include <stdio.h>

#include <QApplication>
#include <QFileInfo>
#include <QTextCodec>
#include <QTextStream>
#include <QtDebug>
#include <unordered_map>

#include "Abini.h"
#include "ageingbox.h"
#include "camerabox.h"
#include "imubox.h"
#include "mainwindow.h"
#include "motorbox.h"
#include "pcbabox.h"
#include "qfreeworkbox.h"
#include "quiescent_current_box.h"
#include "screenbox.h"
#include "wifibox.h"

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
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

    QString folderName = "上位机log";
    QDir dir;

    // 检查并创建目录
    if (!dir.exists(folderName)) {
        if (!dir.mkpath(folderName)) {
            qDebug() << "无法创建目录:" << folderName;
            return;
        }
    }
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
    QString fileName = "上位机日志_" + fileNumber + "_" + QDate::currentDate().toString("yyyy-MM-dd") + ".txt";

    // QString fileName = QDate::currentDate().toString("上位机日志yyyy-MM-dd")
    // +
    // ".txt";
    QString filePath = dir.filePath(folderName + "/" + fileName);

    QFile file(filePath);
    file.open(QIODevice::WriteOnly | QIODevice::Append);
    QTextStream text_stream(&file);
    printf("%s\r\n", msg.toUtf8().constData());  // 或者使用
    fflush(stdout);
    text_stream << message << "\r\n";
    file.close();
}

int main(int argc, char* argv[]) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication a(argc, argv);
    // 设置使用 UTF-8 编码
    QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF-8"));
    // 安装自定义消息处理程序
    qInstallMessageHandler(customMessageHandler);

    // qDebug() << "串口问题"<<QSslSocket::sslLibraryBuildVersionString();
    a.setFont(QFont("Microsoft Yahei", 9));
    QString station = SETTINGS.value("SYSTEM/station").toString();  // 工站
    qDebug() << "工站为：" + station;

    qRegisterMetaType<FacErrorCode>("FacErrorCode");

    // #QUIESCENT_CURRENT
    // #MOTOR_TEST
    // #IMU_CALI
    // #SCREEN_TEST
    // #CAMREA_TEST
    // #WIFIBLE_TEST
    // #AGE_TEST
    // #MAIN_TEST

    std::unordered_map<QString, int> map = {
        {"QUIESCENT_CURRENT", 1}, {"MOTOR_TEST", 2}, {"IMU_CALI", 3},  {"SCREEN_TEST", 4}, {"CAMERA_TEST", 5},
        {"WIFIBLE_TEST", 6},      {"AGE_TEST", 7},   {"PCBA_TEST", 8}, {"FREE_WORK", 9},   {"MAIN_TEST", 10}};

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
            QFreeWorkBox* f = new QFreeWorkBox;  // 自由工站
            f->show();
            f->TotallyTask();
            delete f;
            break;
        }
        case 10: {
            MainWindow h;  // 主测试
            h.show();
            return a.exec();
        }

        default:
            MainWindow h;  // 主测试
            h.show();
            return a.exec();
            break;
    }
}
