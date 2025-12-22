#include "factory_analyzer.h"
#include "qcustomplot.h"
#include "ui_factory_analyzer.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMessageBox>
#include <QMimeData>
#include <QProcess>
#include <QRegExp>
#include <QUrl>
#include <functional>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

factory_analyzer::factory_analyzer(QWidget *parent)
    : QMainWindow(parent),  adb(new Qadb),ui(new Ui::factory_analyzer) {
    ui->setupUi(this);
    setAcceptDrops(true);
    adb->start();
    QCustomPlot *plot_value = new QCustomPlot;
    // 创建压力值曲线图
    plot_value->legend->setVisible(true); // 设置图例可见
    plot_value->xAxis->setRange(0, 1000); // 设置 X 轴范围为 0 到 1000
    plot_value->yAxis->setRange(0, 100);  // 设置 Y 轴范围为 0 到 1000
    plot_value->setInteractions(QCP::iRangeDrag |
                                QCP::iRangeZoom);        // 启用范围拖动和缩放
    plot_value->addGraph();                              // 添加图层
    plot_value->graph(0)->setPen(QPen(Qt::blue));        // 设置图层画笔为蓝色
    plot_value->graph(0)->setBrush(QBrush(Qt::NoBrush)); // 使用蓝色填充
    plot_value->graph(0)->setAntialiased(true);          // 不抗锯齿
    plot_value->graph(0)->setName("电池温度");           // 设置图层名称
    graph_value_vector.push_back(plot_value);
    QVBoxLayout *vlayout = new QVBoxLayout(ui->frame);

    for (uint32_t n = 0; n < graph_value_vector.size(); n++) {
        graph_value_vector[n]->setMinimumHeight(1);
        vlayout->addWidget(graph_value_vector[n]);
    }
    graph_reset(0);

    const int N = 7;
    static const char *namesbiaoqian[N] = {"电池温度", "qcs8625温度", "nsp温度",
                                           "ddr温度",  "bat温度",     "sens温度",
                                           "ntc温度"}; // 添加7条曲线 + 设置颜色
    for (int i = 1; i < N; i++) {
        graph_value_vector[0]->addGraph();

        QColor color = QColor::fromHsv((i * 40) % 360, 255, 220);
        graph_value_vector[0]->graph(i)->setPen(QPen(color, 2));
        graph_value_vector[0]->graph(i)->setName(namesbiaoqian[i]);
    }

    table = ui->tableWidget;
    table->setColumnCount(8);
    QStringList headers = {"消息内容",        "来源", "目标",     "失败/总数",
                           "失败长度/总长度", "通道", "频率(Hz)", "版本",
                           "时间戳(us)"};
    QFont headerFont;
    headerFont.setPointSize(12);
    table->horizontalHeader()->setFont(headerFont);
    table->verticalHeader()->setFont(headerFont);
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setStretchLastSection(true);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);



    const QSize availableSize =
        QApplication::desktop()->availableGeometry(this).size();
    QVariant windowSize(availableSize / 4 * 3);
    this->resize(SETTINGS.value("Window/Size", windowSize).toSize());

    // 构造函数里
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &factory_analyzer::updateAdbStatus);
    timer->start(2000); // 1 秒刷新一次
    adbStatusLabel = new QLabel("ADB连接：<font color='red'>失败</font>");
    ui->statusbar->addPermanentWidget(adbStatusLabel);



    // Tree model
    treeModel = new QStandardItemModel(this);
    treeModel->setHorizontalHeaderLabels({"设备树"});
    ui->treeView->setModel(treeModel);

    // File model
    fileModel = new QStandardItemModel(this);
    fileModel->setHorizontalHeaderLabels({"名字", "大小","类型","日期","权限"});
    ui->tableView->setModel(fileModel);


    // Load root nodes
    loadRoot();

    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->treeView, &QWidget::customContextMenuRequested,
            this, &::factory_analyzer::onTreeViewContextMenu);
    connect(ui->tableView, &QWidget::customContextMenuRequested,
            this, &factory_analyzer::onTableViewContextMenu);


}


void factory_analyzer::adbPull(const QString &remotePathOrigin)
{
    QString remotePath = remotePathOrigin;
    if(remotePath.isEmpty()){
        QMessageBox::warning(this,"错误","远程路径为空！");
        return;
    }

    remotePath.replace("\\","/");

    QString program = "adb";
    QStringList arguments;
    arguments << "pull" << remotePath;

    qDebug() << "[ADB] pull" << arguments;

    QProcess *process = new QProcess(this);
    process->setProcessChannelMode(QProcess::MergedChannels);

    connect(process, QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),
            [process](int exitCode, QProcess::ExitStatus){

                QString out = QDir::currentPath();

                if(exitCode == 0){
                    QMessageBox::information(nullptr,"成功",
                                             "复制完成！数据在：\n" + out);
                    QDesktopServices::openUrl(QUrl::fromLocalFile(out));
                }
                else{
                    QMessageBox::warning(nullptr,"失败",process->readAll());
                }

                process->deleteLater();
            });

    process->start(program, arguments);
}
void factory_analyzer::refreshTreeAfterDelete(const QString &remotePath)
{
    // 找到父路径
    QString parentPath = remotePath;
    int lastSlash = parentPath.lastIndexOf('/');
    if(lastSlash > 0)
        parentPath = parentPath.left(lastSlash);
    else
        parentPath = "/";

    qDebug() << "[TREE] refresh for parent:" << parentPath;

    // 刷根目录
    if(parentPath == "/"){
        loadRoot();
        return;
    }

    // 刷某个子目录
    loadRemoteDirectory(parentPath);
}

void factory_analyzer::loadRemoteDirectory(const QString &path)
{
    if (path.isEmpty())
        return;

    // 使用 Qadb 发送命令
    adb->sendCommand(QString("ls -l %1").arg(path),
                     [this, path](const QString &output, qint64 elapsed) {
                         qDebug() << "[ADB] ls output:" << output;
                         qDebug() << "Elapsed:" << elapsed << "ms";

                         if (output.trimmed().isEmpty()) {
                             qDebug() << "目录为空或不可访问:" << path;
                             return;
                         }

                         // 调用原来的解析函数
                         parseFiles(path, output);
                     }, 5000); // 可选超时时间 5000ms
}


void factory_analyzer::adbDelete(const QString &remotePathOrigin)
{
    QString remotePath = remotePathOrigin;
    if (remotePath.isEmpty()) {
        QMessageBox::warning(this, "错误", "远程路径为空！");
        return;
    }

    if (QMessageBox::question(this,
                              "确认删除",
                              "确定要从设备删除？\n" + remotePath)
        != QMessageBox::Yes)
        return;

    remotePath.replace("\\", "/");

    QString cmd = QString("rm -rf %1").arg(remotePath);
    qDebug() << "[ADB] delete command:" << cmd;

    adb->sendCommand(cmd,
                     [this, remotePath](const QString &output, qint64 elapsed) {
                         qDebug() << "[ADB] delete output:" << output;
                         qDebug() << "Elapsed:" << elapsed << "ms";

                         if (output.contains("No such file") || output.contains("Permission denied")) {
                             QMessageBox::warning(nullptr, "失败", output);
                         } else {
                             QMessageBox::information(nullptr, "成功", "删除成功！");
                             refreshTreeAfterDelete(remotePath);
                         }
                     }, 5000); // 超时时间可调
}


void factory_analyzer::onTreeViewContextMenu(const QPoint &pos)
{
    QModelIndex index = ui->treeView->indexAt(pos);
    if(!index.isValid()) return;

    auto item = treeModel->itemFromIndex(index);
    if(!item) return;

    QString remotePath = item->data(Qt::UserRole + 1).toString();

    QMenu menu(this);

    menu.addAction("复制到本地", [=](){
        adbPull(remotePath);
    });

    menu.addAction("删除远程文件", [=](){
        adbDelete(remotePath);
    });

    menu.exec(ui->treeView->viewport()->mapToGlobal(pos));
}
void factory_analyzer::onTableViewContextMenu(const QPoint &pos)
{
    QModelIndex index = ui->tableView->indexAt(pos);
    if(!index.isValid()) return;

    QString remotePath =
        fileModel->item(index.row(),0)->data(Qt::UserRole + 1).toString();

    QMenu menu(this);

    menu.addAction("复制到本地", [=](){
        adbPull(remotePath);
    });

    menu.addAction("删除远程文件", [=](){
        adbDelete(remotePath);
    });

    menu.exec(ui->tableView->viewport()->mapToGlobal(pos));
}

//     1️⃣ 文件双击进入目录？
//     3️⃣ push / pull 拖拽？
//     4️⃣ Qt progressbar 进度条？
//     5️⃣ 多设备支持？
//     6️⃣ 批量 adb 异步队列？
//     7️⃣ 不阻塞 UI 挂 adb shell -l -a？
// 8️⃣ 打开大目录背景线程？
void factory_analyzer::on_pushButton_13_clicked()
{
    // Load root nodes
    loadRoot();
}

void factory_analyzer::loadRoot()
{
    if (!adb) return;

    adb->sendCommand("ls -1 /", [this](const QString &output, qint64 elapsed) {
        qDebug() << "[ADB] loadRoot output:" << output;
        qDebug() << "Elapsed:" << elapsed << "ms";

        if (output == "ADB不可用") {
            QMessageBox::warning(this, "错误", "设备未连接或ADB不可用！");
            return;
        }

            QStringList remoteList = output.split(QRegExp("[\r\n]+"), Qt::SkipEmptyParts);
        remoteList.sort(Qt::CaseInsensitive);

        // 生成完整路径列表
        QStringList remotePaths;
        for (QString d : remoteList)
            remotePaths.append("/" + d.trimmed());

        // 删除不存在的节点
        for (int row = treeModel->rowCount() - 1; row >= 0; row--) {
            QString existPath = treeModel->item(row)->data(Qt::UserRole + 1).toString();
            if (!remotePaths.contains(existPath))
                treeModel->removeRow(row);
        }

        // 添加缺失节点（有序）
        for (const QString &fullPath : remotePaths) {
            bool found = false;

            for (int i = 0; i < treeModel->rowCount(); i++) {
                if (treeModel->item(i)->data(Qt::UserRole + 1).toString() == fullPath) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                QString name = fullPath.mid(1);
                QStandardItem *item = new QStandardItem(name);
                item->setData(fullPath, Qt::UserRole + 1);
                treeModel->appendRow(item);
            }
        }
    }, 5000); // 5 秒超时，可调整
}



void factory_analyzer::on_treeView_clicked(const QModelIndex &index)
{
    QString path = index.data(Qt::UserRole + 1).toString().trimmed();  // 获取路径
    if (path.isEmpty()) return;

    int childCount = ui->treeView->model()->rowCount(index);
    if (childCount == 0) {
        loadFolder(path); // 异步加载子节点
    }

    if (!adb) return;

    // 使用长连接异步获取目录
    adb->sendCommand(QString("ls -l \"%1\"").arg(path), [this, path](const QString &output, qint64 elapsed){
        qDebug() << "[ADB] ls -l" << path << "elapsed:" << elapsed << "ms";
        if (output == "ADB不可用") {
            QMessageBox::warning(this, "错误", "设备未连接或ADB不可用！");
            return;
        }

        QString trimmedOutput = output.trimmed();
        parseFiles(path, trimmedOutput);
    }, 5000); // 5 秒超时
}


void factory_analyzer::loadFolder(const QString &path)
{
    if (path.isEmpty()) return;

    if (!adb) {
        showlog("ADB不可用！");
        return;
    }

    showlog("loadFolder path = " + path);

    // 使用 adb 长连接异步获取目录
    adb->sendCommand(QString("ls -p \"%1\"").arg(path), [this, path](const QString &output, qint64 elapsed) {
        qDebug() << "[ADB] ls -p" << path << "elapsed:" << elapsed << "ms";

        QString trimmedOutput = output.trimmed();
        if (trimmedOutput.isEmpty()) {
            showlog("folder list is EMPTY!!");
            return;
        }

        QStringList list = trimmedOutput.split(QRegExp("[\r\n]+"), Qt::SkipEmptyParts);

        QModelIndex idx = ui->treeView->currentIndex();
        QStandardItem *parent = treeModel->itemFromIndex(idx);

        if (!parent) {
            showlog("parent IS NULL ERROR!!");
            return;
        }

        for (QString d : list) {
            d = d.trimmed();
            if (!d.endsWith("/")) continue;

            d.chop(1); // 去掉末尾的 /

            auto *child = new QStandardItem(d);
            child->setData(path + "/" + d, Qt::UserRole + 1);
            parent->appendRow(child);
        }


    }, 5000); // 设置超时时间 5 秒
}


void factory_analyzer::parseFiles(QString path, QString data)
{
    fileModel->removeRows(0, fileModel->rowCount());

    QStringList lines = data.split("\n", Qt::SkipEmptyParts);

    for (QString line : lines)
    {
        QStringList s = line.simplified().split(" ");

        if (s.size() < 8)
            continue;

        QString rights = s[0];
        QString size = s[4];
        QString name = s.last();
        QString date = s[5] + " " + s[6] + " " + s[7];
        QString type = rights.startsWith("d") ? "DIR" : "FILE";

        QList<QStandardItem*> row;

        // 名称列
        QStandardItem *nameItem = new QStandardItem(name);

        // 关键！！存储完整路径
        QString fullPath = path.endsWith("/")
                               ? path + name
                               : path + "/" + name;

        nameItem->setData(fullPath, Qt::UserRole + 1);

        row << nameItem;
        row << new QStandardItem(size);
        row << new QStandardItem(type);
        row << new QStandardItem(date);
        row << new QStandardItem(rights);

        fileModel->appendRow(row);
    }
}

void factory_analyzer::updateAdbStatus()
{
    // qDebug() << "[factory_analyzer] 更新 ADB 状态...";

    // 启动 shell（不管是否成功，只是保证进程存在）
    bool started = adb->start();
    // qDebug() << "[factory_analyzer] adb shell start 返回:" << started;

    // 发送测试命令判断 ADB 是否可用
    adb->sendCommand("echo success", [this](const QString &output, qint64 elapsed) {
        // qDebug() << "[factory_analyzer] adb sendCommand 输出:" << output
        //          << ", 耗时:" << elapsed << "ms";

        if (!adbStatusLabel) {
            qDebug() << "[factory_analyzer] adbStatusLabel为空";
            return;
        }
        if (!adbStatusLabel->parent()) {
            qDebug() << "[factory_analyzer] adbStatusLabel 已脱离 UI";
            return;
        }

        QString trimmed = output.trimmed();
        if (trimmed == "success") {
            adbStatusLabel->setText("ADB连接：<font color='green'>成功</font>");
            // qDebug() << "[factory_analyzer] ADB连接成功";
        } else {
            adbStatusLabel->setText("ADB连接：<font color='red'>失败</font>");
            // qDebug() << "[factory_analyzer] ADB连接失败";
        }
    });
}





void factory_analyzer::graph_reset(uint8_t argument) {
    qDebug() << "graph_reset:" << argument;

    for (uint8_t chan = 0; chan < graph_value_vector.size(); chan++) {
        graph_value_vector[chan]->graph(0)->clearData();
    }
}
factory_analyzer::~factory_analyzer() {

    delete ui; }

void factory_analyzer::showlog(const QString &msg) {
    if (msg.isEmpty())
        return;
    if (!ui || !ui->msgEdit)
        return;
    ui->msgEdit->appendPlainText(msg);
    qDebug() << "factory_analyzer" << msg;
}

// --------------------------
// 通用 QProcess 封装
// --------------------------
void factory_analyzer::runProcess(
    const QString &program, const QStringList &arguments,
    const QString &workDir,
    std::function<void(int, QProcess::ExitStatus)> onFinish) {
    QProcess *process = new QProcess(this);

    if (!workDir.isEmpty())
        process->setWorkingDirectory(workDir);

    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QString text = QString::fromLocal8Bit(process->readAllStandardOutput());
        qDebug().noquote() << "[OUTPUT]" << text;
    });

    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        QString text = QString::fromLocal8Bit(process->readAllStandardError());
        qDebug().noquote() << "[ERROR]" << text;
    });

    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [=](int exitCode, QProcess::ExitStatus status) {
                if (onFinish)
                    onFinish(exitCode, status);
                process->deleteLater();
            });

    process->start(program, arguments);
}

// --------------------------
// 批处理执行示例
// --------------------------
void factory_analyzer::on_pushButton_clicked() {
    QString exeDir = QCoreApplication::applicationDirPath();
    QString batDir = exeDir + "/产线调试包v4";
    QString batPath = batDir + "/系统公版拉日志v4.bat";

    if (!QFile::exists(batPath)) {
        QMessageBox::warning(this, "错误", "找不到批处理文件:\n" + batPath);
        return;
    }
  showlog("开始拉日志");
    runProcess("cmd.exe", {"/c", batPath}, batDir,
             [&](int code, QProcess::ExitStatus st) {
      if (st == QProcess::NormalExit && code == 0)
          QMessageBox::information(this, "完成", "脚本执行成功！");
      else
          QMessageBox::warning(this, "错误", "脚本执行失败！");
  });
}

// --------------------------
// 执行 exe 并自动打开报告
// --------------------------
void factory_analyzer::runExeWithReport(const QString &exeName) {
    QString exeDir = QCoreApplication::applicationDirPath();
    QString appDir = exeDir + "/产线调试包v4";
    QString exePath = appDir + "/" + exeName;

    if (!QFile::exists(exePath)) {
        QMessageBox::critical(this, "错误", "未找到程序:\n" + exePath);
        return;
    }

    processOutputLines.clear();

    // 创建进程
    QProcess *process = new QProcess(this);

    // 设置工作目录
    process->setWorkingDirectory(appDir);

    // 设置环境变量（Python UTF-8 输出）
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("PYTHONIOENCODING", "utf-8");
    process->setProcessEnvironment(env);

    // 捕获标准输出
    connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
        QByteArray data = process->readAllStandardOutput();
        QString text = QString::fromUtf8(data);
        qDebug().noquote() << "[分析输出] " << text;
        processOutputLines.append(
            text.split(QRegExp("[\r\n]"), Qt::SkipEmptyParts));
    });

    // 捕获标准错误
    connect(process, &QProcess::readyReadStandardError, this, [=]() {
        QByteArray data = process->readAllStandardError();
        QString text = QString::fromUtf8(data);
        qDebug().noquote() << "[分析错误] " << text;
        processOutputLines.append(
            text.split(QRegExp("[\r\n]"), Qt::SkipEmptyParts));
    });

    // 程序结束回调
    connect(
        process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, [=](int exitCode, QProcess::ExitStatus status) {
            if (status == QProcess::NormalExit && exitCode == 0) {
                QMessageBox::information(this, "完成", "程序执行成功！");

                // 匹配报告文件
                QString reportFile;
                QRegExp rx(
                    "报告保存在[:：]\\s*(\\S+\\.txt)"); // 非贪婪匹配，支持中文/英文冒号
                for (QString line : processOutputLines) {
                    line = line.trimmed();
                    if (rx.indexIn(line) != -1) {
                        reportFile = rx.cap(1);
                        break;
                    }
                }

                if (!reportFile.isEmpty()) {
                    reportFile =
                        QFileInfo(appDir + "/" + reportFile).absoluteFilePath();
                    if (QFile::exists(reportFile)) {
                        qDebug() << "自动打开分析报告：" << reportFile;
                        QMetaObject::invokeMethod(
                            this,
                            [reportFile]() {
                                QDesktopServices::openUrl(QUrl::fromLocalFile(reportFile));
                            },
                            Qt::QueuedConnection);
                    } else {
                        qDebug() << "分析报告不存在：" << reportFile;
                    }
                } else {
                    qDebug() << "未找到分析报告文件" << reportFile << appDir << exeName;
                }
            } else {
                QMessageBox::warning(
                    this, "错误", QString("程序执行结束，返回码=%1").arg(exitCode));
            }

            process->deleteLater();
        });

    // 启动进程
    process->start(exePath);

    if (!process->waitForStarted(3000)) {
        QMessageBox::critical(this, "错误", "程序启动失败！");
        return;
    }

    qDebug() << "程序开始执行，工作目录:" << appDir;
}

void factory_analyzer::on_pushButton_2_clicked() {
    showlog("开始分析产线问题");
    runExeWithReport("分析产线问题v4.exe");
}
void factory_analyzer::on_pushButton_5_clicked() {
    showlog("开始分析日志详情");
    runExeWithReport("日志解释器.exe");
}

// --------------------------
// 修复清除日志失败
// --------------------------
void factory_analyzer::on_pushButton_3_clicked() {

    showlog("开始修复清除日志失败");
    QString exeDir = QCoreApplication::applicationDirPath();
    QString batDir = exeDir + "/产线调试包v4";
    QString batPath = batDir + "/修复清除日志失败问题.bat";

    if (!QFile::exists(batPath)) {
        QMessageBox::warning(this, "错误", "找不到批处理文件:\n" + batPath);
        return;
    }

    runProcess("cmd.exe", {"/c", batPath}, batDir,
               [&](int code, QProcess::ExitStatus st) {
        if (st == QProcess::NormalExit && code == 0)
            QMessageBox::information(this, "完成", "脚本执行成功！");
        else
            QMessageBox::warning(this, "错误", "脚本执行失败！");
    });
}

// --------------------------
// 拖拽文件
// --------------------------
void factory_analyzer::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        event->ignore();
}

void factory_analyzer::dropEvent(QDropEvent *event) {
    QList<QUrl> urls = event->mimeData()->urls();
    if (ui->label->geometry().contains(event->pos())) {
        for (const QUrl &url : urls) {
            QString filePath = url.toLocalFile();
            qDebug() << "拖到 Label 上的文件:" << filePath;

            // 调用推送函数
            pushFileToGaoTongDevice(filePath);
        }
    }
    if (ui->label_1->geometry().contains(event->pos())) {
        for (const QUrl &url : urls) {
            QString filePath = url.toLocalFile();
            qDebug() << "拖到 label_1 上的文件:" << filePath;

            // 调用推送函数
            pushFileToZiYanDevice(filePath);
        }
    }
}

// --------------------------
// 推送文件到 adb 设备
// --------------------------
void factory_analyzer::pushFileToZiYanDevice(const QString &localFile) {
    // 组合 adb 命令
    QFileInfo fi(localFile);
    QString fileName = fi.fileName(); // 拿到拖入文件的名字

    // 组合 adb 命令，推送到 /usr/bin/<fileName>，然后修改该文件权限
    QString cmd =
        QString("adb remount && "
                          "adb push %1 /system/bin/%2 && "
                          "adb shell chmod 777 /system/bin/%2")
                      .arg(QDir::toNativeSeparators(localFile)) // 转成 Windows 原生路径
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
    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [=](int exitCode, QProcess::ExitStatus status) {
                if (status == QProcess::NormalExit && exitCode == 0) {
                    qDebug().noquote() << "ADB 执行完成";
                    showlog("ADB 执行成功");
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

void factory_analyzer::pushFileToGaoTongDevice(const QString &localFile) {
    // 组合 adb 命令
    QFileInfo fi(localFile);
    QString fileName = fi.fileName(); // 拿到拖入文件的名字

    // 组合 adb 命令，推送到 /usr/bin/<fileName>，然后修改该文件权限
    QString cmd =
        QString("adb shell mount -o rw,remount / && "
                          "adb push %1 /usr/bin/%2 && "
                          "adb shell chmod 777 /usr/bin/%2")
                      .arg(QDir::toNativeSeparators(localFile)) // 转成 Windows 原生路径
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
    connect(process,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [=](int exitCode, QProcess::ExitStatus status) {
                if (status == QProcess::NormalExit && exitCode == 0) {
                    qDebug().noquote() << "ADB 执行完成";
                        showlog("ADB 执行成功");
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

// --------------------------
// 批量执行 adb 命令（按钮4）
void factory_analyzer::on_pushButton_4_clicked() {
    showlog("机器开始进老化");
    QString cmd = "test_mp_stage.sh erase &&  "
                  "test_mp_stage.sh aging_test 1800 &&  reboot";


    adb->sendCommand(cmd, [](const QString &output, qint64 elapsed) {
        qDebug() << "Command finished, elapsed:" << elapsed << "ms";
        qDebug() << "Output:" << output;
    }, 15000); // 设置长一些的超时，比如 15 秒



}

// --------------------------
// 同步设备时间
// --------------------------
void factory_analyzer::on_pushButton_6_clicked() {
    QString exeDir = QCoreApplication::applicationDirPath();
    QString appDir = exeDir + "/产线调试包v4";
    QString batPath = appDir + "/sync_time_v2.bat";

    if (!QFile::exists(batPath)) {
        showlog("sync_time_v2.bat 不存在！");
        return;
    }

    showlog("开始同步机器时间，请保持机器唤醒USB连接");

    // 执行 bat 同步时间
    runProcess(batPath, {}, appDir, [this](int, QProcess::ExitStatus) {
        // 获取设备时间
        QString cmd = "date"; // 或 "adb shell date" 看你的 sendCommand 实现

        adb->sendCommand(cmd, [this](const QString &output, qint64 elapsed) {
            qDebug() << "Command finished, elapsed:" << elapsed << "ms";
            qDebug() << "Output:" << output;

            QString time = output.trimmed();

            showlog("设置完成，当前设备时间: " + time);
            showlog("产线电脑时间和真实时间有一定差异，所以会差一两分钟等");
        }, 15000); // 超时 15 秒
    });
}

void deleteDirContent(const QString &path)
{
    QDir dir(path);

    if (!dir.exists())
        return;

    QFileInfoList list = dir.entryInfoList(
        QDir::NoDotAndDotDot |
            QDir::AllEntries,
        QDir::DirsFirst);

    for (const QFileInfo &info : list)
    {
        if (info.isDir())
        {
            // 递归删除子目录内容
            deleteDirContent(info.filePath());
            dir.rmdir(info.filePath());
        }
        else
        {
            QFile::remove(info.filePath());
        }
    }
}
void factory_analyzer::on_pushButton_8_clicked() {

    QString basePath = R"(产线调试包v4/log/)";
deleteDirContent(basePath);

}
void factory_analyzer::on_pushButton_7_clicked() {
    QString basePath = R"(产线调试包v4/log/)";
    QDir dir(basePath);
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::Name | QDir::Reversed);

    QString latestFolder;
    for (const QFileInfo &info : dir.entryInfoList()) {
        if (info.fileName().endsWith("_")) {
            latestFolder = info.absoluteFilePath();
            break;
        }
    }
    if (latestFolder.isEmpty())
        return;

    QString logFilePath = latestFolder + "/system/system/system.log";

    QFile file(logFilePath);
    if (!file.open(QFile::ReadOnly | QFile::Text))
        return;

    auto plot = graph_value_vector[0];
    plot->clearGraphs();

    //---------------------------------------
    // 1. 7 条曲线名称
    //---------------------------------------
    const int N = 7;
    static const char *namesbiaoqian[N] = {"电池温度", "qcs8625温度", "nsp温度",
                                           "ddr温度",  "bat温度",     "sens温度",
                                           "ntc温度"};
    static const char *names[N] = {"电池温度",
                                   "qcs8625_temperature_group",
                                   "nsp_temperature_group",
                                   "ddr_temperature_group",
                                   "bat_temperature_group",
                                   "sens_temperature_group",
                                   "ntc_temperature_group"};
    // 添加7条曲线 + 设置颜色
    for (int i = 0; i < N; i++) {
        plot->addGraph();

        QColor color = QColor::fromHsv((i * 40) % 360, 255, 220);
        plot->graph(i)->setPen(QPen(color, 2));
        plot->graph(i)->setName(namesbiaoqian[i]);
    }

    //---------------------------------------
    // 2. 正则列表
    //---------------------------------------
    QRegularExpression reg_bat(R"(DUSS7F.*temperature:(\d+))");

    QRegularExpression reg_group(
        R"((qcs8625_temperature_group|nsp_temperature_group|ddr_temperature_group|bat_temperature_group|sens_temperature_group|ntc_temperature_group).*<->\s*(\d+))");

    //---------------------------------------
    // 3. 独立时间轴
    //---------------------------------------
    double ts[N] = {0};

    QString line;

    //---------------------------------------
    // 4. 解析日志
    //---------------------------------------
    while (!file.atEnd()) {
        line = file.readLine();

        // ① 电池温度（你的原始需求）
        auto m1 = reg_bat.match(line);
        if (m1.hasMatch()) {
            double tempC = m1.captured(1).toInt() / 10.0;
            plot->graph(0)->addData(ts[0]++, tempC);
            continue;
        }

        // ② 其它6个温度组
        auto m2 = reg_group.match(line);
        if (m2.hasMatch()) {

            QString groupName = m2.captured(1);
            double tempC = m2.captured(2).toInt() / 10.0;

            for (int i = 1; i < N; i++) {
                if (groupName == names[i]) {
                    plot->graph(i)->addData(ts[i]++, tempC);
                    break;
                }
            }
        }
    }

    //---------------------------------------
    // 5. 显示曲线
    //---------------------------------------
    plot->rescaleAxes(true);
    plot->replot();
}
void factory_analyzer::updateForwardTable()
{
    if (!adb) return;

    qDebug().noquote() << "ADB updateForwardTable";

    // 使用长连接 shell
    adb->sendCommand("duss_shell stat --show forward", [this](const QString &output, qint64 elapsed) {
        qDebug() << "[ADB] updateForwardTable elapsed:" << elapsed << "ms";

        if (output == "ADB不可用") {
            QMessageBox::warning(this, "错误", "设备未连接或ADB不可用！");
            return;
        }

        // 清空表格
        table->setRowCount(0);

        // 按行解析输出
        QStringList lines = output.split(QRegExp("[\r\n]+"), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            if (line.contains("----msg:")) {
                parseAndAddLine(line);
            }
        }
    }, 5000); // 5 秒超时，可根据情况调整
}

QString translateId(const QString &raw)
{    // 在类里或者函数外定义映射表
    const QMap<QString, QString> serviceMap = {
                                               {"0x0800", "media_server_liveview"},
                                               {"0x0801", "system_service"},
                                               {"0x0802", "upgrade_service"},
                                               {"0x0803", "amt_service"},
                                               {"0x0804", "sec_service"},
                                               {"0x1e00", "dji_mb_ctrl_duss_shell"},
                                               {"0x1d00", "dji_blackbox_oopl_hms"},
                                               {"lo:1001", "dji_config_store"},
                                               {"0x0806", "ite_service"},
                                               {"0x0700", "wifi_service"},
                                               {"0x0701", "uav_slide_window"},
                                               {"0x0904", "sdr_agent"},
                                               {"0x0907", "wireless_manager_wlm"},
                                               {"0x0100", "camera"},
                                               {"0x0104", "body_cpu_bcpu"},
                                               {"0x0105", "display_engine"},
                                               {"0x0103", "icdco_display_engine"},
                                               {"0x0106", "imu_data"},
                                               {"0x0107", "camera_download/imu data"},
                                               {"0x0164", "liveview_observer"},
                                               {"0x0167", "camera_test"},
                                               {"0x1200", "perception_rtos"},
                                               {"0x1201", "perception_udp"},
                                               {"0x1204", "dji_perception"},
                                               {"0x1206", "dji_perception_x"},
                                               {"0x1207", "ss_dspmanager"},
                                               {"0x1102", "auto_flight"},
                                               {"0x1103", "autotest"},
                                               {"0x0300", "flyctrl"},
                                               {"0x0306", "flyctrl_param"},
                                               {"0x0050", "esc"},
                                               {"0x0c01", "esc_1"},
                                               {"0x0c02", "esc_2"},
                                               {"0x0c03", "esc_3"},
                                               {"0x1100", "machine_learning"},
                                               {"0x1105", "nnserver"},
                                               {"0x1106", "machine_learning_test"},
                                               {"0x0400", "gimbal"},
                                               {"0x0b00", "battery"},
                                               {"0x0500", "charging_dock"},
                                               {"0x1704", "dcs_service"},
                                               {"0x0a00", "PC DA2 调参"},
                                               {"0x0a01", "PC DA2 基础：升级、日志"},
                                               {"0x1c00", "dji_gui_on_disp"},
                                               {"0x0200", "app"},
                                               };
    // raw 形如 "0x0000:0x0802"
    QStringList parts = raw.split(':');
    if (parts.size() != 2)
        return raw; // 异常情况直接返回原始

    QString id = parts[1]; // 取后半部分
    QString name = serviceMap.value(id, id); // 映射表查找
    return QString("%1:%2").arg(name, id);   // upgrade_service:0x0802
}
void factory_analyzer::parseAndAddLine(const QString &line) {
    static QRegularExpression rx("----msg:0x([0-9A-Fa-f]+)\\s+"
                                 "from\\s+([0-9A-Fa-fx:]+)\\s+"
                                 "to\\s+([0-9A-Fa-fx:]+)\\s+"
                                 "fail/all:([0-9]+)/([0-9]+)\\s+"
                                 "fail_len/all_len:([0-9]+)/([0-9]+)Bytes\\s+"
                                 "receive\\s+from\\s+channel:(\\d+)\\s+"
                                 "freq:([0-9.]+)hz\\s+"
                                 "forward_ver:(\\d+)\\s+"
                                 "final\\s+msg\\s+timestamp:(\\d+)us",
                                 QRegularExpression::DotMatchesEverythingOption);

    QRegularExpressionMatch m = rx.match(line);

    if (!m.hasMatch()) {

        showlog("[NO MATCH 2]" +line);
        return;
    }

    QString msgId = m.captured(1);
    // QString from = m.captured(2);
    // QString to = m.captured(3);
    // 使用映射表
    // 使用
    QString fromRaw = m.captured(2); // 例如 "0x0000:0x0802"
    QString toRaw   = m.captured(3); // 例如 "0x0000:0x0200"

    QString fromName = translateId(fromRaw);
    QString toName   = translateId(toRaw);

    QString fail = m.captured(4) + "/" + m.captured(5);
    QString failLen = m.captured(6) + "/" + m.captured(7);
    QString channel = m.captured(8);
    QString freq = m.captured(9);
    QString ver = m.captured(10);
    QString ts = m.captured(11);
    auto createItem = [](const QString &text) {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter); // 居中
        QFont f = item->font();
        f.setBold(true); // 字体加粗
        item->setFont(f);
        return item;
    };

    int row = table->rowCount();
    table->insertRow(row);

    table->setItem(row, 0, createItem(msgId));
    // table->setItem(row, 1, createItem(from));
    // table->setItem(row, 2, createItem(to));
    // 可以显示在表格里
    table->setItem(row, 1, createItem(fromName));
    table->setItem(row, 2, createItem(toName));

    table->setItem(row, 3, createItem(fail));
    table->setItem(row, 4, createItem(failLen));
    table->setItem(row, 5, createItem(channel));
    table->setItem(row, 6, createItem(freq));
    table->setItem(row, 7, createItem(ver));
    table->setItem(row, 8, createItem(ts));

    // -----------------
    // 高亮必须放在 setItem 之后
    // -----------------
    bool highlight = (m.captured(4).toInt() != 0); // fail > 0 则红色

    if (highlight) {
        for (int c = 0; c < 9; ++c) {
            QTableWidgetItem *it = table->item(row, c);
            if (it) // 避免崩溃
                it->setBackground(QBrush(QColor("#ffe0e0")));

        }
    }
    table->resizeColumnsToContents();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

}

void factory_analyzer::on_pushButton_9_clicked() {
    updateForwardTable(); // 启动时立即更新
}
void factory_analyzer::closeEvent(QCloseEvent *) {
    SETTINGS.setValue("Window/Size", this->size());
    ddr_press = false;
}
void factory_analyzer::runCmd(const QString &cmd)
{
    showlog("[RUN] " + cmd);

    QProcess p;
    p.start(cmd);

    // 不要阻塞 UI
    while (!p.waitForFinished(100)) {
        QCoreApplication::processEvents();
        if(ddr_press==false){
             showlog("退出等待");
             return;
        }

    }

    QString output = p.readAllStandardOutput();
    QString err = p.readAllStandardError();

    if (!output.isEmpty()) showlog(output);
    if (!err.isEmpty()) showlog("[ERR] " + err);
}


void factory_analyzer::on_pushButton_10_clicked()
{
    const int LOOP = 1;
    ddr_press = true;
    showlog("\n===== START PRESS REBOOT TEST =====");

    for (int i = 1; i <= LOOP; i++)
    {
        showlog(QString(">>> ROUND %1 / %2").arg(i).arg(LOOP));
        runCmd("adb wait-for-device");
        if(ddr_press==false)
        {
            showlog("退出PRESS REBOOT");
            return;
        }


        // 1. 重启
        runCmd("adb shell reboot");
        if(ddr_press==false)
        {
            showlog("退出PRESS REBOOT");
            return;
        }
        // 2. 等待设备上线
        runCmd("adb wait-for-device");

        showlog(QString(">>> DONE REBOOT ROUND %1").arg(i));
        showlog("-----------------------------");
        if(ddr_press==false)
        {
            showlog("退出PRESS REBOOT");
            return;
        }
    }

    runCmd("adb wait-for-device");


    if(ddr_press==false)
    {
        showlog("退出PRESS REBOOT");
        return;
    }
    showlog("\n===== REBOOT PRESS FINISHED =====");
    showlog("===== START FINAL STEPS =====");

    // 3. 修改关机配置
    QString cmd = "simulate_device -s DevicePowerUserIdleControlAutoShutdownTime 0";

    adb->sendCommand(cmd, [](const QString &output, qint64 elapsed) {
        qDebug() << "Command finished, elapsed:" << elapsed << "ms";
        qDebug() << "Output:" << output;
    }, 15000); // 设置长一些的超时，比如 15 秒

    showlog(">>> Set AutoShutdownTime = 0");

    on_pushButton_11_clicked();

    showlog("===== PASS =====");
}



void factory_analyzer::on_pushButton_11_clicked()
{

    QString cmd = "test_mp_stage.sh erase && test_mp_stage.sh ddr_press 1800 &&reboot ";

adb->sendCommand(cmd, [](const QString &output, qint64 elapsed) {
    qDebug() << "Command finished, elapsed:" << elapsed << "ms";
    qDebug() << "Output:" << output;
}, 15000); // 设置长一些的超时，比如 15 秒


}


void factory_analyzer::on_pushButton_12_clicked()
{
     ddr_press = false;

}





void factory_analyzer::on_pushButton_14_clicked()
{
    adb->sendCommand("cat /proc/version", [](const QString &output, qint64 elapsed){
        qDebug() << "Command output:" << output;
        qDebug() << "Elapsed:" << elapsed << "ms";
    });
}

void factory_analyzer::on_pushButton_15_clicked()
{
    adb->sendCommand("reboot", [](const QString &output, qint64 elapsed){
        qDebug() << "Command output:" << output;
        qDebug() << "Elapsed:" << elapsed << "ms";
    });
}
void factory_analyzer::displayCmdline(QTableWidget *table, const QString &cmdline) {
    // 清空旧数据
    table->clear();
    table->setRowCount(0);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels(QStringList() << "参数" << "值");

    // 字体加粗 lambda
    auto createItem = [](const QString &text) {
        QTableWidgetItem *item = new QTableWidgetItem(text);
        item->setTextAlignment(Qt::AlignCenter);
        QFont f = item->font();
        f.setBold(true);
        item->setFont(f);
        return item;
    };

    // 按空格拆分参数
    QStringList params = cmdline.split(' ', Qt::SkipEmptyParts);

    // 遍历每个参数 key=value
    for (const QString &p : params) {
        int idx = p.indexOf('=');
        QString key = p;
        QString value;
        if (idx != -1) {
            key = p.left(idx);
            value = p.mid(idx + 1);
        }

        int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, createItem(key));
        table->setItem(row, 1, createItem(value));

        // 高亮重要字段
        if (
            key.contains("mp_state", Qt::CaseInsensitive) ||
            key.contains("production_sn", Qt::CaseInsensitive) ||
            key.contains("board_sn", Qt::CaseInsensitive) ||
            key.contains("androidboot.secure_debug", Qt::CaseInsensitive) ||
              key.contains("loglevel", Qt::CaseInsensitive) ||
            key.contains("boot_mode", Qt::CaseInsensitive)) {
            for (int c = 0; c < 2; ++c) {
                QTableWidgetItem *it = table->item(row, c);
                if (it)
                    it->setBackground(QColor("#ffe0e0")); // 浅红背景
            }
        }
    }

    table->resizeColumnsToContents();
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
}

void factory_analyzer::on_pushButton_16_clicked()
{
    adb->sendCommand("cat /proc/cmdline", [this](const QString &output, qint64 elapsed){
        qDebug() << "Command output:" << output;
        qDebug() << "Elapsed:" << elapsed << "ms";

        // 去掉多余换行和空格
        QString cmdline = output;
        cmdline = cmdline.simplified(); // 去掉多余空格/换行，连续空白合并为一个空格

        displayCmdline(ui->tableWidget_2, cmdline);
    });
}

