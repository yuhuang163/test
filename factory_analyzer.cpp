#include "factory_analyzer.h"
#include "ui_factory_analyzer.h"
#include <QProcess>
#include <QDir>
#include <QMessageBox>
#include <QDebug>
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif


#include <QMimeData>
factory_analyzer::factory_analyzer(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::factory_analyzer)
{
    ui->setupUi(this);
    // 允许主窗口接收拖放
    setAcceptDrops(true);

}

factory_analyzer::~factory_analyzer()
{
    delete ui;
}

void factory_analyzer::on_pushButton_clicked()
{
    // 获取 exe 所在目录
    QString exeDir = QCoreApplication::applicationDirPath();

    // 拼接 bat 文件路径
    QString batDir = exeDir + "/产线调试包v4";           // 工作目录
    QString batPath = batDir + "/系统公版拉日志v4.bat";  // bat 文件完整路径

    // 是否存在
    if (!QFile::exists(batPath)) {
        QMessageBox::warning(this, "错误", "找不到批处理文件:\n" + batPath);
        return;
    }

    // 创建 QProcess
    QProcess *process = new QProcess(this);

    // 设置工作目录为 bat 所在目录
    process->setWorkingDirectory(batDir);

    // 监听标准输出
    connect(process, &QProcess::readyReadStandardOutput, [=]() {
        QByteArray data = process->readAllStandardOutput();
        QString text = QString::fromLocal8Bit(data);  // 解决中文乱码
        qDebug().noquote() << "[BAT OUTPUT] " << text;
    });

    // 监听标准错误
    connect(process, &QProcess::readyReadStandardError, [=]() {
        QByteArray data = process->readAllStandardError();
        QString text = QString::fromLocal8Bit(data);
        qDebug().noquote() << "[BAT ERROR] " << text;
    });

    // 监听执行完成
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus status) {
                qDebug() << "BAT 执行结束，exitCode =" << exitCode
                         << " status =" << status;

                if (status == QProcess::NormalExit && exitCode == 0) {
                    QMessageBox::information(this, "完成", "系统公版拉日志脚本执行成功！");
                } else if (status == QProcess::NormalExit && exitCode != 0) {
                    QMessageBox::warning(this, "完成", QString("脚本执行结束，但返回码 = %1，可能执行失败！").arg(exitCode));
                } else if (status == QProcess::CrashExit) {
                    QMessageBox::critical(this, "错误", "脚本执行崩溃！");
                }

                process->deleteLater();
            });

    // 启动批处理（非阻塞）
    process->start("cmd.exe", QStringList() << "/c" << batPath);

    if (!process->waitForStarted(3000)) {
        QMessageBox::critical(this, "错误", "批处理文件启动失败！");
        return;
    }

    qDebug() << "BAT 开始执行:" << batPath;
}

void factory_analyzer::on_pushButton_2_clicked()
{
    QString exeDir = QCoreApplication::applicationDirPath();
    QString appDir = exeDir + "/产线调试包v4";
    QString exePath = appDir + "/分析产线问题v4.exe";

    if (!QFile::exists(exePath)) {
        QMessageBox::critical(this, "错误", "未找到产线分析程序:\n" + exePath);
        return;
    }

    processOutputLines.clear(); // 清空上次输出

    QProcess *process = new QProcess(this);
    process->setProgram(exePath);
    process->setWorkingDirectory(appDir);

    // 设置 Python UTF-8 输出
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    process->setProcessEnvironment(env);

    // 捕获标准输出
    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray data = process->readAllStandardOutput();
        QString text = QString::fromUtf8(data);
        qDebug().noquote() << "[分析输出] " << text;
        processOutputLines.append(text.split(QRegExp("[\r\n]"), Qt::SkipEmptyParts));
    });

    // 捕获标准错误
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        QByteArray data = process->readAllStandardError();
        QString text = QString::fromUtf8(data);
        qDebug().noquote() << "[分析错误] " << text;
        processOutputLines.append(text.split(QRegExp("[\r\n]"), Qt::SkipEmptyParts));
    });

    // 监听程序结束
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int exitCode, QProcess::ExitStatus status) {

                if (status == QProcess::NormalExit && exitCode == 0) {
                    QMessageBox::information(this, "完成", "产线问题分析程序执行成功！");


                    // 找报告文件
                    QString reportFile;
                    QRegExp rx("报告保存在：(.+\\.txt)");
                    for (const QString &line : processOutputLines) {
                        if (rx.indexIn(line) != -1) {
                            reportFile = rx.cap(1).trimmed();
                            break;
                        }
                    }

                    if (!reportFile.isEmpty()) {
                        // 加上工作目录前缀，得到绝对路径
                        reportFile = appDir + "/" + reportFile;

                        if (QFile::exists(reportFile)) {
                            qDebug() << "自动打开分析报告：" << reportFile;
                            QMetaObject::invokeMethod(this, [reportFile](){
                                QDesktopServices::openUrl(QUrl::fromLocalFile(reportFile));
                            }, Qt::QueuedConnection);
                        } else {
                            qDebug() << "分析报告文件不存在：" << reportFile;
                        }
                    } else {
                        qDebug() << "未找到分析报告文件，无法打开。";
                    }

                } else if (status == QProcess::NormalExit && exitCode != 0) {
                    QMessageBox::warning(this, "完成", QString("程序执行结束，但返回码=%1").arg(exitCode));
                } else if (status == QProcess::CrashExit) {
                    QMessageBox::critical(this, "错误", "程序崩溃！");
                }

                process->deleteLater();
            });

    process->start();

    if (!process->waitForStarted(3000)) {
        QMessageBox::critical(this, "错误", "程序启动失败！");
        return;
    }

    qDebug() << "分析程序开始执行，工作目录:" << appDir;
}
void factory_analyzer::on_pushButton_3_clicked()
{
    // 获取 exe 所在目录
    QString exeDir = QCoreApplication::applicationDirPath();

    // 拼接 bat 文件路径
    QString batDir = exeDir + "/产线调试包v4";  // BAT 所在目录
    QString batPath = batDir + "/修复清除日志失败问题.bat";

    // 检查是否存在
    if (!QFile::exists(batPath)) {
        QMessageBox::warning(this, "错误", "找不到批处理文件:\n" + batPath);
        return;
    }

    // 创建 QProcess
    QProcess *process = new QProcess(this);

    // 设置工作目录为 BAT 所在目录
    process->setWorkingDirectory(batDir);

    // 捕获标准输出
    connect(process, &QProcess::readyReadStandardOutput, [=]() {
        QByteArray data = process->readAllStandardOutput();
        QString text = QString::fromLocal8Bit(data); // GBK 中文解码
        qDebug().noquote() << "[BAT OUTPUT] " << text;
    });

    // 捕获标准错误
    connect(process, &QProcess::readyReadStandardError, [=]() {
        QByteArray data = process->readAllStandardError();
        QString text = QString::fromLocal8Bit(data);
        qDebug().noquote() << "[BAT ERROR] " << text;
    });

    // 进程结束回调
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            [=](int exitCode, QProcess::ExitStatus status) {
                qDebug() << "BAT 执行结束，exitCode =" << exitCode << " status =" << status;

                if (status == QProcess::NormalExit && exitCode == 0) {
                    QMessageBox::information(this, "完成", "修复清除日志失败问题脚本执行成功！");
                } else if (status == QProcess::NormalExit && exitCode != 0) {
                    QMessageBox::warning(this, "完成", QString("脚本执行结束，但返回码 = %1，可能执行失败！").arg(exitCode));
                } else if (status == QProcess::CrashExit) {
                    QMessageBox::critical(this, "错误", "脚本执行崩溃！");
                }

                process->deleteLater();
            });

    // 启动 BAT（非阻塞）
    process->start("cmd.exe", QStringList() << "/c" << batPath);

    if (!process->waitForStarted(3000)) {
        QMessageBox::critical(this, "错误", "批处理文件启动失败！");
        return;
    }

    qDebug() << "BAT 开始执行:" << batPath;
}


// 拖入事件
void factory_analyzer::dragEnterEvent(QDragEnterEvent *event)
{
    // 只接受文件
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    } else {
        event->ignore();
    }
}

// 放下事件
void factory_analyzer::dropEvent(QDropEvent *event)
{
    // 检查是否落在 QLabel 区域
    if (ui->label->geometry().contains(event->pos())) {
        QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl &url : urls) {
            QString filePath = url.toLocalFile();
            qDebug() << "拖到 Label 上的文件:" << filePath;

            // 调用推送函数
            pushFileToGaoTongDevice(filePath);
        }
    }

    if (ui->label_1->geometry().contains(event->pos())) {
        QList<QUrl> urls = event->mimeData()->urls();
        for (const QUrl &url : urls) {
            QString filePath = url.toLocalFile();
            qDebug() << "拖到 label_1 上的文件:" << filePath;

            // 调用推送函数
            pushFileToZiYanDevice(filePath);
        }
    }

}


void factory_analyzer::pushFileToZiYanDevice(const QString &localFile)
{
    // 组合 adb 命令
    QFileInfo fi(localFile);
    QString fileName = fi.fileName(); // 拿到拖入文件的名字

    // 组合 adb 命令，推送到 /usr/bin/<fileName>，然后修改该文件权限
    QString cmd = QString(
                      "adb remount && "
                      "adb push %1 /system/bin/%2 && "
                      "adb shell chmod 777 /system/bin/%2"
                      ).arg(QDir::toNativeSeparators(localFile))  // 转成 Windows 原生路径
                      .arg(fileName);

    qDebug().noquote() << "[ cmd]" << cmd;
    QProcess *process = new QProcess(this);

    // 捕获标准输出
    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray data = process->readAllStandardOutput();
        QString text = QString::fromLocal8Bit(data); // GBK 中文解码
        qDebug().noquote() << "[ADB OUTPUT]" << text;
    });

    // 捕获错误输出
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        QByteArray data = process->readAllStandardError();
        QString text = QString::fromLocal8Bit(data);
        qDebug().noquote() << "[ADB ERROR]" << text;
    });

    // 进程结束回调
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int exitCode, QProcess::ExitStatus status) {
                if (status == QProcess::NormalExit && exitCode == 0) {
                    qDebug().noquote() << "ADB 执行完成";
                } else {
                    qDebug().noquote() << "ADB 执行失败, exitCode=" << exitCode;
                }
                process->deleteLater();
            });

    // 启动进程
    process->start("cmd.exe", QStringList() << "/c" << cmd);

    if (!process->waitForStarted(3000)) {
        qDebug().noquote() << "ADB 进程启动失败";
    } else {
        qDebug().noquote() << "ADB 开始执行...";
    }
}

void factory_analyzer::pushFileToGaoTongDevice(const QString &localFile)
{
    // 组合 adb 命令
    QFileInfo fi(localFile);
    QString fileName = fi.fileName(); // 拿到拖入文件的名字

    // 组合 adb 命令，推送到 /usr/bin/<fileName>，然后修改该文件权限
    QString cmd = QString(
                      "adb shell mount -o rw,remount / && "
                      "adb push %1 /usr/bin/%2 && "
                      "adb shell chmod 777 /usr/bin/%2"
                      ).arg(QDir::toNativeSeparators(localFile))  // 转成 Windows 原生路径
                      .arg(fileName);

  qDebug().noquote() << "[ cmd]" << cmd;
    QProcess *process = new QProcess(this);

    // 捕获标准输出
    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray data = process->readAllStandardOutput();
        QString text = QString::fromLocal8Bit(data); // GBK 中文解码
        qDebug().noquote() << "[ADB OUTPUT]" << text;
    });

    // 捕获错误输出
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        QByteArray data = process->readAllStandardError();
        QString text = QString::fromLocal8Bit(data);
        qDebug().noquote() << "[ADB ERROR]" << text;
    });

    // 进程结束回调
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int exitCode, QProcess::ExitStatus status) {
                if (status == QProcess::NormalExit && exitCode == 0) {
                    qDebug().noquote() << "ADB 执行完成";
                } else {
                    qDebug().noquote() << "ADB 执行失败, exitCode=" << exitCode;
                }
                process->deleteLater();
            });

    // 启动进程
    process->start("cmd.exe", QStringList() << "/c" << cmd);

    if (!process->waitForStarted(3000)) {
        qDebug().noquote() << "ADB 进程启动失败";
    } else {
        qDebug().noquote() << "ADB 开始执行...";
    }
}

void factory_analyzer::on_pushButton_4_clicked()
{
    // 组合命令
    QString cmd = "adb shell test_mp_stage.sh erase && "
                  "adb shell test_mp_stage.sh aging_test 1800 && "
                  "adb shell reboot";

    QProcess *process = new QProcess(this);

    // 捕获标准输出
    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray data = process->readAllStandardOutput();
        QString text = QString::fromLocal8Bit(data); // GBK 中文解码
        qDebug().noquote() << "[ADB OUTPUT]" << text;
    });

    // 捕获错误输出
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        QByteArray data = process->readAllStandardError();
        QString text = QString::fromLocal8Bit(data);
        qDebug().noquote() << "[ADB ERROR]" << text;
    });

    // 进程结束回调
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int exitCode, QProcess::ExitStatus status){
                if (status == QProcess::NormalExit && exitCode == 0) {
                    qDebug().noquote() << "ADB 执行完成";
                } else {
                    qDebug().noquote() << "ADB 执行失败, exitCode=" << exitCode;
                }
                process->deleteLater();
            });

    // 启动进程
    process->start("cmd.exe", QStringList() << "/c" << cmd);

    if (!process->waitForStarted(3000)) {
        qDebug().noquote() << "ADB 进程启动失败";
    } else {
        qDebug().noquote() << "ADB 开始执行...";
    }
}



void factory_analyzer::on_pushButton_5_clicked()
{

    QString exeDir = QCoreApplication::applicationDirPath();
    QString appDir = exeDir + "/产线调试包v4";
    QString exePath = appDir + "/日志解释器.exe";

    if (!QFile::exists(exePath)) {
        QMessageBox::critical(this, "错误", "未找到产线分析程序:\n" + exePath);
        return;
    }

    processOutputLines.clear(); // 清空上次输出

    QProcess *process = new QProcess(this);
    process->setProgram(exePath);
    process->setWorkingDirectory(appDir);

    // 设置 Python UTF-8 输出
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    process->setProcessEnvironment(env);

    // 捕获标准输出
    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray data = process->readAllStandardOutput();
        QString text = QString::fromUtf8(data);
        qDebug().noquote() << "[分析输出] " << text;
        processOutputLines.append(text.split(QRegExp("[\r\n]"), Qt::SkipEmptyParts));
    });

    // 捕获标准错误
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        QByteArray data = process->readAllStandardError();
        QString text = QString::fromUtf8(data);
        qDebug().noquote() << "[分析错误] " << text;
        processOutputLines.append(text.split(QRegExp("[\r\n]"), Qt::SkipEmptyParts));
    });

    // 监听程序结束
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int exitCode, QProcess::ExitStatus status) {

                if (status == QProcess::NormalExit && exitCode == 0) {
                    QMessageBox::information(this, "完成", "产线问题分析程序执行成功！");


                    // 找报告文件
                    QString reportFile;
                    QRegExp rx("报告保存在：(.+\\.txt)");
                    for (const QString &line : processOutputLines) {
                        if (rx.indexIn(line) != -1) {
                            reportFile = rx.cap(1).trimmed();
                            break;
                        }
                    }

                    if (!reportFile.isEmpty()) {
                        // 加上工作目录前缀，得到绝对路径
                        reportFile = appDir + "/" + reportFile;

                        if (QFile::exists(reportFile)) {
                            qDebug() << "自动打开分析报告：" << reportFile;
                            QMetaObject::invokeMethod(this, [reportFile](){
                                QDesktopServices::openUrl(QUrl::fromLocalFile(reportFile));
                            }, Qt::QueuedConnection);
                        } else {
                            qDebug() << "分析报告文件不存在：" << reportFile;
                        }
                    } else {
                        qDebug() << "未找到分析报告文件，无法打开。";
                    }

                } else if (status == QProcess::NormalExit && exitCode != 0) {
                    QMessageBox::warning(this, "完成", QString("程序执行结束，但返回码=%1").arg(exitCode));
                } else if (status == QProcess::CrashExit) {
                    QMessageBox::critical(this, "错误", "程序崩溃！");
                }

                process->deleteLater();
            });

    process->start();

    if (!process->waitForStarted(3000)) {
        QMessageBox::critical(this, "错误", "程序启动失败！");
        return;
    }

    qDebug() << "分析程序开始执行，工作目录:" << appDir;
}

void factory_analyzer::showlog(QString msg) {
    if (msg.isEmpty()) {
        qDebug() << "showlog: 收到空消息，跳过日志输出";
        return;
    }

    if (!ui || !ui->msgEdit) {
        qDebug() << "showlog: ui 或 msgEdit 未初始化";
        return;
    }

    ui->msgEdit->appendPlainText(msg);
    qDebug() << "factory_analyzer的" << msg;
}

void factory_analyzer::on_pushButton_6_clicked()
{
    // 获取路径
    QString exeDir = QCoreApplication::applicationDirPath();
    QString appDir = exeDir + "/产线调试包v4";
    QString batPath = appDir + "/sync_time_v2.bat";

    if (!QFile::exists(batPath)) {
        showlog("sync_time_v2.bat 不存在！");
        return;
    }

    // 创建进程
    QProcess *process = new QProcess(this);
    process->setWorkingDirectory(appDir);

    // 捕获输出（可选）
    // connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
    //     QString out = QString::fromLocal8Bit(process->readAllStandardOutput());
    //     showlog(out);
    // });

    // connect(process, &QProcess::readyReadStandardError, this, [=]() {
    //     QString out = QString::fromLocal8Bit(process->readAllStandardError());
    //     showlog(out);
    // });

    // 当 bat 执行完毕后，再查询设备时间
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [=](int exitCode, QProcess::ExitStatus status)
            {
                Q_UNUSED(exitCode)
                Q_UNUSED(status)

                // ------------------------------
                // 再执行 adb shell date 获取时间
                // ------------------------------

                QProcess *adb = new QProcess(this);
                QStringList args;
                args << "shell" << "date" ;

                connect(adb, &QProcess::readyReadStandardOutput, this, [=]() {
                    QString time = QString::fromLocal8Bit(adb->readAllStandardOutput()).trimmed();
                    showlog("设置完成，当前设备时间: " + time);
                });

                adb->start("adb", args);
            });
    showlog("开始同步机器时间，请保持机器唤醒usb连接");
    // 启动 bat
    process->start(batPath);
}


