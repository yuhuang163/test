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
void factory_analyzer::on_pushButton_14_clicked() { // 启动按键监控

    // shell->sendCommand(
    //     "echo 哈哈哈",
    //     [this](const QString &output, qint64 elapsed)  {
    //         qDebug() << "Elapsed:" << elapsed << "ms"<<output;
    //         showlog("完成");
    //     }
    //     ,3000);

    QByteArray data;
    // data.append(char(0x00)); // 你的命令数据
    QByteArray pkt = bulk->buildPacket(0x28, 2, 0, 1, data);
    //55 0D 04 33 0A 28 15 58 40 00 01 A8 3C
    //55 0d 04 33 0a 28 00 00 20 00 01 00 ef c9
    //55 0c 04 f7 0a 28 00 00 20 00 01 d5 0f
    //55 0d 04 33 0a 28 00 00 20 00 01 3f 9b
    bulk->bulkWrite(0x05, pkt, 1000); // 发到 OUT endpoint 0x01，100ms超时
    // QByteArray data;
    // if (bulk->bulkRead(0x85, data,1000)) // 0x81 是 IN 端点
    //     qDebug() << "Received:" << data.toHex();

    // shell->sendCommand(
    //     "cd E:/垃圾",
    //     [this](const QString &output, qint64 elapsed) {
    //         qDebug() << "Elapsed:" << elapsed << "ms" << output;
    //         showlog("完成");
    //     },
    //     3000);

    // shell->sendCommand(
    //     "[Console]::OutputEncoding",
    //     [this](const QString &output, qint64 elapsed) {
    //         qDebug() << "Elapsed:" << elapsed << "ms" << output;
    //         showlog("完成");
    //     },
    //     3000);

    // QProcess p;

    // QString ps =
    //     "[Console]::OutputEncoding = "
    //     "[System.Text.UTF8Encoding]::new(); "
    //     "echo 哈哈哈";

    // p.start(
    //     "powershell",
    //     {
    //         "-NoProfile",
    //         "-NonInteractive",
    //         "-Command",
    //         ps
    //     }
    //     );

    // p.waitForFinished();

    // QByteArray raw = p.readAllStandardOutput();

    // // ✅ UTF-8 解码（现在是对的）
    // QString output = QString::fromUtf8(raw);

    // qDebug().noquote() << "OUTPUT:" << output;
}
factory_analyzer::factory_analyzer(QWidget *parent)
    : QMainWindow(parent), bulk(new QBulk),
adb(new Qadb),
    shell(new Qshell), shellMonitor(new Qshell), ui(new Ui::factory_analyzer) {
    ui->setupUi(this);
    setAcceptDrops(true);
    adb->start();
    shell->start();
    shellMonitor->start();


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

    usbStatusLabel = new QLabel("usb状态：<font color='red'>wait</font>");
    ui->statusbar->addPermanentWidget(usbStatusLabel);

    bulkStatusLabel = new QLabel("bulk连接：<font color='red'>wait</font>");
    ui->statusbar->addPermanentWidget(bulkStatusLabel);


    // Tree model
    treeModel = new QStandardItemModel(this);
    treeModel->setHorizontalHeaderLabels({"设备树"});
    ui->treeView->setModel(treeModel);

    // File model
    fileModel = new QStandardItemModel(this);
    fileModel->setHorizontalHeaderLabels(
        {"名字", "大小", "类型", "日期", "权限"});
    ui->tableView->setModel(fileModel);

    // Load root nodes
    loadRoot();

    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->treeView, &QWidget::customContextMenuRequested, this,
            &::factory_analyzer::onTreeViewContextMenu);
    connect(ui->tableView, &QWidget::customContextMenuRequested, this,
            &factory_analyzer::onTableViewContextMenu);

    ui->lineEdit->installEventFilter(this);

    execAdb(
        "--version",
        [this](const QString &output, qint64 elapsed) {
        qDebug() << "Elapsed:" << elapsed << "ms\n" << output;
        showlog(output);
        },
        3000);


   setupUSB();
    connect(bulk, SIGNAL(sendGetDjiResponse(int)), this, SLOT(solveGetDjiResponse(int)));
    connect(bulk, SIGNAL(send_bulk_data(QString)), this, SLOT(refreshbulkData(QString)));
}
void factory_analyzer::solveGetDjiResponse(int data) {
    if(data==1)
        showlog("收到设备处理回应成功");
    else
        showlog("收到设备处理回应失败");

    getRespone = data;
}
void factory_analyzer::setupUSB()
{    bulkreadThread = new QThread(this);
    reconnectTimer = new QTimer(this);
    // 1️⃣ 移动 bulk 到线程
    bulk->moveToThread(bulkreadThread);

    // 2️⃣ 线程启动 -> 阻塞读
    connect(bulkreadThread, &QThread::started, bulk, &QBulk::startRead);

    // 3️⃣ 数据到 UI
    // connect(bulk, &QBulk::readyRead, this, [this]( QByteArray &data){
    //     qDebug() << "USB RX:" << data.toHex();
    //     bulk->parseCmd( data);
    // });

    // 4️⃣ 错误处理
    connect(bulk, &QBulk::error, this, [this](int code, const QString &e){
        qDebug() << "bulk error:" << e;
    bulkStatusLabel->setText(
        "bulk连接：: <font color='red'>失败</font>");
        reconnectTimer->start();
    });

    // 5️⃣ 重连定时器
    reconnectTimer->setInterval(1000); // 每秒尝试一次
    connect(reconnectTimer, &QTimer::timeout, this, &factory_analyzer::tryOpenUSB);

    // 6️⃣ 尝试先打开一次
    tryOpenUSB();



}
    void factory_analyzer::refreshbulkData(QString data) { showlog(data); }
void factory_analyzer::tryOpenUSB() {
    if (!bulk->isOpen()) {
        if (bulk->openDevice(0x2CA3, 0x0025, 4)) {
            qDebug() << "bulk 已连接";
            bulkStatusLabel->setText(
                QString("bulk连接：: <font color='green'>成功</font>"));

            qDebug() << "bulk连接：: OK";
            bulkreadThread->start();

            reconnectTimer->stop();
        } else {
            qDebug() << "bulk 打开失败，等待重连";
            reconnectTimer->start();
        }
    }
}

// 在你的类里，比如 factory_analyzer
void factory_analyzer::execAdb(
    const QString &args,
    std::function<void(const QString &output, qint64 elapsed)> callback,
    int timeout) {
    QStringList commands = args.split("&&", Qt::SkipEmptyParts);
    QStringList fullCommands;
    for (auto &c : commands) {
        QString trimmed = c.trimmed();
        if (!trimmed.isEmpty()) {
            fullCommands << QString("./factorydebugv4/adb/adb.exe %1").arg(trimmed);
        }
    }

    // 用 ; 拼接成单条命令在 shell 执行
    QString cmd = fullCommands.join(" ; ");

    shell->sendCommand(
        cmd,
        [callback](const QString &output, qint64 elapsed) {
            if (callback)
                callback(output, elapsed);
        },
        timeout);
}
QString factory_analyzer::execAdbBlocking(const QString &args, int timeout) {
    QStringList commands = args.split("&&", Qt::SkipEmptyParts);
    QStringList fullCommands;

    for (auto &c : commands) {
        QString trimmed = c.trimmed();
        if (!trimmed.isEmpty()) {
            fullCommands << QString("./factorydebugv4/adb/adb.exe %1").arg(trimmed);
        }
    }

    // 用 ; 拼接成单条命令在 shell 执行
    QString cmd = fullCommands.join(" ; ");

    QString result;
    QEventLoop loop;

    // 使用 sendCommand 的异步接口，但通过事件循环阻塞
    shell->sendCommand(
        cmd,
        [&](const QString &output, qint64 /*elapsed*/) {
        result = output;
        loop.quit(); // 命令执行完成，退出阻塞
        },
        timeout);

    loop.exec(); // 阻塞等待命令完成
    return result;
}

void factory_analyzer::adbPull(const QString &remotePathOrigin) {
    QString remotePath = remotePathOrigin;
    if (remotePath.isEmpty()) {
        QMessageBox::warning(this, "错误", "远程路径为空！");
        return;
    }

    remotePath.replace("\\", "/");

    QString cmd = QString("pull %1").arg(remotePath);
    execAdb(cmd, [](const QString &output, qint64 elapsed) {
        QString out = QDir::currentPath(); // 保存路径

        if (output.contains("error", Qt::CaseInsensitive)) {
            QMessageBox::warning(nullptr, "失败", output);
        } else {
            QMessageBox::information(nullptr, "成功", "复制完成！数据在：\n" + out);
            QDesktopServices::openUrl(QUrl::fromLocalFile(out));
        }

        qDebug() << "[ADB pull] elapsed:" << elapsed << "ms";
    });
}
void factory_analyzer::refreshTreeAfterDelete(const QString &remotePath) {
    // 找到父路径
    QString parentPath = remotePath;
    int lastSlash = parentPath.lastIndexOf('/');
    if (lastSlash > 0)
        parentPath = parentPath.left(lastSlash);
    else
        parentPath = "/";

    qDebug() << "[TREE] refresh for parent:" << parentPath;

    // 刷根目录
    if (parentPath == "/") {
        loadRoot();
        return;
    }

    // 刷某个子目录
    loadRemoteDirectory(parentPath);
}

void factory_analyzer::loadRemoteDirectory(const QString &path) {
    if (path.isEmpty())
        return;

    // 使用 Qadb 发送命令
    adb->sendCommand(
        QString("ls -l %1").arg(path),
        [this, path](const QString &output, qint64 elapsed) {
            qDebug() << "[ADB] ls output:" << output;
            qDebug() << "Elapsed:" << elapsed << "ms";

            if (output.trimmed().isEmpty()) {
                qDebug() << "目录为空或不可访问:" << path;
                return;
            }

            // 调用原来的解析函数
            parseFiles(path, output);
        },
        5000); // 可选超时时间 5000ms
}

void factory_analyzer::adbDelete(const QString &remotePathOrigin) {
    QString remotePath = remotePathOrigin;
    if (remotePath.isEmpty()) {
        QMessageBox::warning(this, "错误", "远程路径为空！");
        return;
    }

    if (QMessageBox::question(this, "确认删除",
                              "确定要从设备删除？\n" + remotePath) !=
        QMessageBox::Yes)
        return;

    remotePath.replace("\\", "/");

    QString cmd = QString("rm -rf %1").arg(remotePath);
    qDebug() << "[ADB] delete command:" << cmd;

    adb->sendCommand(
        cmd,
        [this, remotePath](const QString &output, qint64 elapsed) {
            qDebug() << "[ADB] delete output:" << output;
            qDebug() << "Elapsed:" << elapsed << "ms";

            if (output.contains("No such file") ||
                output.contains("Permission denied")) {
                QMessageBox::warning(nullptr, "失败", output);
            } else {
                QMessageBox::information(nullptr, "成功", "删除成功！");
                refreshTreeAfterDelete(remotePath);
            }
        },
        5000); // 超时时间可调
}

void factory_analyzer::onTreeViewContextMenu(const QPoint &pos) {
    QModelIndex index = ui->treeView->indexAt(pos);
    if (!index.isValid())
        return;

    auto item = treeModel->itemFromIndex(index);
    if (!item)
        return;

    QString remotePath = item->data(Qt::UserRole + 1).toString();

    QMenu menu(this);

    menu.addAction("复制到本地", [=]() { adbPull(remotePath); });

    menu.addAction("删除远程文件", [=]() { adbDelete(remotePath); });

    menu.exec(ui->treeView->viewport()->mapToGlobal(pos));
}
void factory_analyzer::onTableViewContextMenu(const QPoint &pos) {
    QModelIndex index = ui->tableView->indexAt(pos);
    if (!index.isValid())
        return;

    QString remotePath =
        fileModel->item(index.row(), 0)->data(Qt::UserRole + 1).toString();

    QMenu menu(this);

    menu.addAction("复制到本地", [=]() { adbPull(remotePath); });

    menu.addAction("删除远程文件", [=]() { adbDelete(remotePath); });

    menu.exec(ui->tableView->viewport()->mapToGlobal(pos));
}

//     1️⃣ 文件双击进入目录？
//     3️⃣ push / pull 拖拽？
//     4️⃣ Qt progressbar 进度条？
//     5️⃣ 多设备支持？
//     6️⃣ 批量 adb 异步队列？
//     7️⃣ 不阻塞 UI 挂 adb shell -l -a？
// 8️⃣ 打开大目录背景线程？
void factory_analyzer::on_pushButton_13_clicked() {
    // Load root nodes
    loadRoot();
}

void factory_analyzer::loadRoot() {
    if (!adb)
        return;

    adb->sendCommand(
        "ls -1 /",
        [this](const QString &output, qint64 elapsed) {
        qDebug() << "[ADB] loadRoot output:" << output;
        qDebug() << "Elapsed:" << elapsed << "ms";

        if (output == "ADB shell被中断") {
            QMessageBox::warning(this, "错误", "设备未连接或ADB不可用！");
            return;
        }

        QStringList remoteList =
            output.split(QRegExp("[\r\n]+"), Qt::SkipEmptyParts);
        remoteList.sort(Qt::CaseInsensitive);

        // 生成完整路径列表
        QStringList remotePaths;
        for (QString d : remoteList)
            remotePaths.append("/" + d.trimmed());

        // 删除不存在的节点
        for (int row = treeModel->rowCount() - 1; row >= 0; row--) {
            QString existPath =
                treeModel->item(row)->data(Qt::UserRole + 1).toString();
            if (!remotePaths.contains(existPath))
                treeModel->removeRow(row);
        }

        // 添加缺失节点（有序）
        for (const QString &fullPath : remotePaths) {
            bool found = false;

            for (int i = 0; i < treeModel->rowCount(); i++) {
                if (treeModel->item(i)->data(Qt::UserRole + 1).toString() ==
                    fullPath) {
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
        },
        5000); // 5 秒超时，可调整
}

void factory_analyzer::on_tableView_doubleClicked(const QModelIndex &index) {
    if (!index.isValid() || !adb || !fileModel)
        return;

    // 1. 从 UserRole 取远端完整路径（唯一可信来源）
    QString remotePath =
        fileModel->item(index.row(), 0)->data(Qt::UserRole + 1).toString();

    if (remotePath.isEmpty())
        return;

    // 2. adb shell cat（注意加引号）
    QString cmd = QString("cat \"%1\"").arg(remotePath);

    adb->sendCommand(
        cmd,
        [this, remotePath](const QString &output, qint64 elapsed) {
            ui->msgEdit->appendPlainText(
                QString("===== %1 (%2 ms) =====").arg(remotePath).arg(elapsed));

            ui->msgEdit->appendPlainText(output);
            ui->msgEdit->appendPlainText("\n");
        },
        5000);
}

void factory_analyzer::on_treeView_clicked(const QModelIndex &index) {
    QString path = index.data(Qt::UserRole + 1).toString().trimmed(); // 获取路径
    if (path.isEmpty())
        return;

    int childCount = ui->treeView->model()->rowCount(index);
    if (childCount == 0) {
        loadFolder(path); // 异步加载子节点
    }

    if (!adb)
        return;

    ui->treeView->expand(index);
    // 使用长连接异步获取目录
    adb->sendCommand(
        QString("ls -l \"%1\"").arg(path),
        [this, path](const QString &output, qint64 elapsed) {
        qDebug() << "[ADB] ls -l" << path << "elapsed:" << elapsed << "ms";
        if (output == "ADB不可用") {
            QMessageBox::warning(this, "错误", "设备未连接或ADB不可用！");
            return;
        }

        QString trimmedOutput = output.trimmed();
        parseFiles(path, trimmedOutput);
        },
        5000); // 5 秒超时
}

void factory_analyzer::loadFolder(const QString &path) {
    if (path.isEmpty())
        return;

    if (!adb) {
        showlog("ADB不可用！");
        return;
    }

    showlog("loadFolder path = " + path);

    // 使用 adb 长连接异步获取目录
    adb->sendCommand(
        QString("ls -p \"%1\"").arg(path),
        [this, path](const QString &output, qint64 elapsed) {
        qDebug() << "[ADB] ls -p" << path << "elapsed:" << elapsed << "ms";

        QString trimmedOutput = output.trimmed();
        if (trimmedOutput.isEmpty()) {
            showlog("folder list is EMPTY!!");
            return;
        }

        QStringList list =
            trimmedOutput.split(QRegExp("[\r\n]+"), Qt::SkipEmptyParts);

        QModelIndex idx = ui->treeView->currentIndex();
        QStandardItem *parent = treeModel->itemFromIndex(idx);

        if (!parent) {
            showlog("parent IS NULL ERROR!!");
            return;
        }

        for (QString d : list) {
            d = d.trimmed();
            if (!d.endsWith("/"))
                continue;

            d.chop(1); // 去掉末尾的 /

            auto *child = new QStandardItem(d);
            child->setData(path + "/" + d, Qt::UserRole + 1);
            parent->appendRow(child);
        }
        },
        5000); // 设置超时时间 5 秒
}

void factory_analyzer::parseFiles(QString path, QString data) {
    fileModel->removeRows(0, fileModel->rowCount());

    QStringList lines = data.split("\n", Qt::SkipEmptyParts);

    for (QString line : lines) {
        QStringList s = line.simplified().split(" ");

        if (s.size() < 8)
            continue;

        QString rights = s[0];
        QString size = s[4];
        QString name = s.last();
        QString date = s[5] + " " + s[6] + " " + s[7];
        QString type = rights.startsWith("d") ? "DIR" : "FILE";

        QList<QStandardItem *> row;

        // 名称列
        QStandardItem *nameItem = new QStandardItem(name);

        // 关键！！存储完整路径
        QString fullPath = path.endsWith("/") ? path + name : path + "/" + name;

        nameItem->setData(fullPath, Qt::UserRole + 1);

        row << nameItem;
        row << new QStandardItem(size);
        row << new QStandardItem(type);
        row << new QStandardItem(date);
        row << new QStandardItem(rights);

        fileModel->appendRow(row);
    }
}

void factory_analyzer::updateAdbStatus() {
    // qDebug() << "[factory_analyzer] 更新 ADB 状态...";

    updateBatteryLevel();

    updateQualcommComStatus();

    adb->startKeyMonitorAdbShell("/dev/input/event1",
                                 [this](const QString &keyName) {
                                     showlog(QString("%1 被按下").arg(keyName));
                                 });

    // 启动 shell（不管是否成功，只是保证进程存在）
    bool started = adb->start();
    // qDebug() << "[factory_analyzer] adb shell start 返回:" << started;

    // 发送测试命令判断 ADB 是否可用
    adb->sendCommand(
        "echo success", [this](const QString &output, qint64 elapsed) {
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

    if (bulkreadThread && bulkreadThread->isRunning()) {
        bulk->stopRead();
        bulkreadThread->quit();
        bulkreadThread->wait();
        bulkreadThread->deleteLater();
    }

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
  //   QString exeDir = QCoreApplication::applicationDirPath();
  //   QString batDir = exeDir + "/factorydebugv4";
  //   QString batPath = batDir + "/系统公版拉日志v4.bat";

  //   if (!QFile::exists(batPath)) {
  //       QMessageBox::warning(this, "错误", "找不到批处理文件:\n" + batPath);
  //       return;
  //   }
  // showlog("开始拉日志");
  //   runProcess("cmd.exe", {"/c", batPath}, batDir,
  //            [&](int code, QProcess::ExitStatus st) {
  //     if (st == QProcess::NormalExit && code == 0)
  //         QMessageBox::information(this, "完成", "脚本执行成功！");
  //     else
  //         QMessageBox::warning(this, "错误", "脚本执行失败！");
  // });

  showlog("开始拉日志");
  // 时间戳
  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");

  // 1️⃣ 一次性执行获取 device_id + ls /blackbox
  QString cmd = "deviceId=$(cat /factory_data/device_id.txt); echo "
                "DEVICE_ID=$deviceId; ls /blackbox";

  adb->sendCommand(
      cmd,
      [this, timestamp](const QString &output, qint64) {
          QStringList lines = output.split('\n', Qt::SkipEmptyParts);

          // 2️⃣ 解析 deviceId
          QString deviceId;
          QStringList flightDirs;
          for (const QString &line : lines) {
          if (line.startsWith("DEVICE_ID=")) {
                  deviceId = line.mid(QString("DEVICE_ID=").length()).trimmed();
          } else if (line.startsWith("flight")) {
              flightDirs << line.trimmed();
          }
          }
        qDebug() << "deviceId" << deviceId << "lines:" << lines
                   << "output:" << output;
          // 3️⃣ 创建固定目录

        QString logPath = "./factorydebugv4/log/" + timestamp + "_" + deviceId;
        QStringList dirs = {"system", "camera", "gui", "amt",
                            "aging_test_result"};
        QDir().mkpath(logPath);
        for (const QString &d : dirs) {
          QDir().mkpath(logPath + "/" + d);
        }

        // 4️⃣ 拉固定模块
        for (const QString &d : dirs) {
          QString fullCmd = QString("cd %1; %3 pull /blackbox/%2; cd ../../../")
                                  .arg(logPath, d, "../../adb/adb.exe");
          shell->sendCommand(
              fullCmd,
              [logPath, this](const QString &out, qint64 elapsed) {
                  qDebug() << "[pullDirsSequential] 完成:" << out
                           << "耗时:" << elapsed << "ms";
                  showlog("完成:" + out);
              },
              100000);
        }

        // 5️⃣ 拉 flightXXXX 目录
        for (const QString &flight : flightDirs) {
          QString localDir = logPath + "/" + flight;
          QDir().mkpath(localDir);

          QString fullCmd = QString("cd %1; %3 pull /blackbox/%2; cd ../../../")
                                .arg(localDir, flight, "../../adb/adb.exe");
          shell->sendCommand(
              fullCmd,
              [this](const QString &out, qint64 elapsed) {
                  qDebug() << "[flightXXXX] 完成:" << out << "耗时:" << elapsed
                           << "ms";
                  showlog("完成:" + out);
              },
              10000);
        }
      },
      10000);
}

// --------------------------
// 执行 exe 并自动打开报告
// --------------------------
void factory_analyzer::runExeWithReport(const QString &exeName) {
    QString exeDir = QCoreApplication::applicationDirPath();
    QString appDir = exeDir + "/factorydebugv4";
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
    QString batDir = exeDir + "/factorydebugv4";
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
// void factory_analyzer::pushFileToZiYanDevice(const QString &localFile) {
//     // 组合 adb 命令
//     QFileInfo fi(localFile);
//     QString fileName = fi.fileName(); // 拿到拖入文件的名字

//     // 组合 adb 命令，推送到 /usr/bin/<fileName>，然后修改该文件权限
//     QString cmd =
//         QString("adb remount && "
//                           "adb push %1 /system/bin/%2 && "
//                           "adb shell chmod 777 /system/bin/%2")
//                       .arg(QDir::toNativeSeparators(localFile)) // 转成
//                       Windows 原生路径 .arg(fileName);

//     qDebug().noquote() << "[ cmd]" << cmd;
//     QProcess *process = new QProcess(this);

//     // 捕获标准输出
//     connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
//         QByteArray data = process->readAllStandardOutput();
//         QString text = QString::fromLocal8Bit(data); // GBK 中文解码
//         qDebug().noquote() << "[ADB OUTPUT]" << text;
//     });

//     // 捕获错误输出
//     connect(process, &QProcess::readyReadStandardError, this, [=]() {
//         QByteArray data = process->readAllStandardError();
//         QString text = QString::fromLocal8Bit(data);
//         qDebug().noquote() << "[ADB ERROR]" << text;
//     });

//     // 进程结束回调
//     connect(process,
//             QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
//             this,
//             [=](int exitCode, QProcess::ExitStatus status) {
//                 if (status == QProcess::NormalExit && exitCode == 0) {
//                     qDebug().noquote() << "ADB 执行完成";
//                     showlog("ADB 执行成功");
//                 } else {
//                     qDebug().noquote() << "ADB 执行失败, exitCode=" <<
//                     exitCode;
//                 }
//                 process->deleteLater();
//             });

//     // 启动进程
//     process->start("cmd.exe", QStringList() << "/c" << cmd);

//     if (!process->waitForStarted(3000)) {
//         qDebug().noquote() << "ADB 进程启动失败";
//     } else {
//         qDebug().noquote() << "ADB 开始执行...";
//     }
// }
void factory_analyzer::pushFileToZiYanDevice(const QString &localFile) {
    QFileInfo fi(localFile);
    QString fileName = fi.fileName(); // 文件名

    // 将多条操作写成一次 adb shell 命令
    // 注意：路径要用双引号包裹，防止空格问题
    QString cmd =
        QString("remount && "
                          "push %1 /system/bin/%2 && "
                          "shell chmod 777 /system/bin/%2")
                      .arg(QDir::toNativeSeparators(localFile)) // 转成 Windows 原生路径
                      .arg(fileName);

    qDebug().noquote() << "[ADB CMD]" << cmd;

    // 调用统一封装的 execAdb
    execAdb(cmd, [this, fileName](const QString &output, qint64 elapsed) {
        qDebug().noquote() << QString("ADB 执行耗时: %1 ms").arg(elapsed);
        qDebug().noquote() << "[ADB OUTPUT]\n" << output;

        if (output.contains("error", Qt::CaseInsensitive)) {
            showlog(QString("推送 %1 失败").arg(fileName));
            qDebug().noquote() << "ADB 执行失败";
        } else {
            showlog(QString("推送 %1 成功").arg(fileName));
            qDebug().noquote() << "ADB 执行成功";
        }
    });
}
void factory_analyzer::pushFileToGaoTongDevice(const QString &localFile) {
    QFileInfo fi(localFile);
    QString fileName = fi.fileName(); // 文件名

    QString cmd =
        QString("shell mount -o rw,remount / && "
                          "push %1 /usr/bin/%2 && "
                          "shell chmod 777 /usr/bin/%2")
                      .arg(QDir::toNativeSeparators(localFile)) // 转成 Windows 原生路径
                      .arg(fileName);

    qDebug().noquote() << "[ADB CMD]" << cmd;

    // 调用统一封装的 execAdb
    execAdb(cmd, [this, fileName](const QString &output, qint64 elapsed) {
        qDebug().noquote() << QString("ADB 执行耗时: %1 ms").arg(elapsed);
        qDebug().noquote() << "[ADB OUTPUT]\n" << output;

        if (output.contains("error", Qt::CaseInsensitive)) {
            showlog(QString("推送 %1 失败").arg(fileName));
            qDebug().noquote() << "ADB 执行失败";
        } else {
            showlog(QString("推送 %1 成功").arg(fileName));
            qDebug().noquote() << "ADB 执行成功";
        }
    });
}
// void factory_analyzer::pushFileToGaoTongDevice(const QString &localFile) {
//     // 组合 adb 命令
//     QFileInfo fi(localFile);
//     QString fileName = fi.fileName(); // 拿到拖入文件的名字

//     // 组合 adb 命令，推送到 /usr/bin/<fileName>，然后修改该文件权限
//     QString cmd =
//         QString("adb shell mount -o rw,remount / && "
//                           "adb push %1 /usr/bin/%2 && "
//                           "adb shell chmod 777 /usr/bin/%2")
//                       .arg(QDir::toNativeSeparators(localFile)) // 转成
//                       Windows 原生路径 .arg(fileName);

//   qDebug().noquote() << "[ cmd]" << cmd;
//     QProcess *process = new QProcess(this);

//   // 捕获标准输出
//     connect(process, &QProcess::readyReadStandardOutput, this, [=]() {
//         QByteArray data = process->readAllStandardOutput();
//         QString text = QString::fromLocal8Bit(data); // GBK 中文解码
//         qDebug().noquote() << "[ADB OUTPUT]" << text;
//     });

//     // 捕获错误输出
//     connect(process, &QProcess::readyReadStandardError, this, [=]() {
//         QByteArray data = process->readAllStandardError();
//         QString text = QString::fromLocal8Bit(data);
//         qDebug().noquote() << "[ADB ERROR]" << text;
//     });

//     // 进程结束回调
//     connect(process,
//             QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
//             this,
//             [=](int exitCode, QProcess::ExitStatus status) {
//                 if (status == QProcess::NormalExit && exitCode == 0) {
//                     qDebug().noquote() << "ADB 执行完成";
//                     showlog("ADB 执行成功");
//                 } else {
//                     qDebug().noquote() << "ADB 执行失败, exitCode=" <<
//                     exitCode;
//                 }
//                 process->deleteLater();
//             });

//     // 启动进程
//     process->start("cmd.exe", QStringList() << "/c" << cmd);

//     if (!process->waitForStarted(3000)) {
//         qDebug().noquote() << "ADB 进程启动失败";
//     } else {
//         qDebug().noquote() << "ADB 开始执行...";
//     }
// }

// --------------------------
// 批量执行 adb 命令（按钮4）
void factory_analyzer::on_pushButton_4_clicked() {
    showlog("机器开始进老化");
    QString cmd = "test_mp_stage.sh erase &&  "
                  "test_mp_stage.sh aging_test 1800 &&  reboot";

    adb->sendCommand(
        cmd,
        [](const QString &output, qint64 elapsed) {
        qDebug() << "Command finished, elapsed:" << elapsed << "ms";
        qDebug() << "Output:" << output;
        },
        15000); // 设置长一些的超时，比如 15 秒
}

// --------------------------
// 同步设备时间
// --------------------------
void factory_analyzer::on_pushButton_6_clicked() {
    // 获取本地时间
    QDateTime now = QDateTime::currentDateTime();
    QString times = now.toString("yyyy.MM.dd-HH:mm:ss"); // BusyBox格式

    showlog("同步设备时间: " + times);

    // adb 命令列表
    QString cmd = QString("busybox date -s %1 && "
                          "busybox hwclock -w")
                      .arg(times);

    // 使用你现有的 sendCommand
    adb->sendCommand(cmd, [this](const QString &output, qint64) {
        showlog("设置完成，当前设备时间: " + output);
    });
}

void deleteDirContent(const QString &path) {
    QDir dir(path);
    if (!dir.exists()) {
        qDebug() << "[deleteDir] not exist:" << path;
        return;
    }

    QFileInfoList list = dir.entryInfoList(
        QDir::NoDotAndDotDot | QDir::AllEntries, QDir::DirsFirst);

    for (const QFileInfo &info : list) {
        const QString fullPath = info.filePath();

        if (info.isDir()) {
            // 只清空子目录内容，不在这里删目录
            deleteDirContent(fullPath);
        } else {
            if (!QFile::remove(fullPath)) {
                qDebug() << "[deleteDir] remove file FAILED:" << fullPath;
            }
        }
    }
    bool ok = QDir(path).removeRecursively();

    if (!ok) {
        qDebug() << "[removeDirSafe] FAILED:" << path;
    } else {
        qDebug() << "[removeDirSafe] OK:" << path;
    }
}
void factory_analyzer::on_pushButton_8_clicked() {
  showlog("开始删除本地日志 ");
    QString basePath = R"(factorydebugv4/log/)";
  deleteDirContent(basePath);
    showlog("删除完成");
}
void factory_analyzer::on_pushButton_7_clicked() {
    QString basePath = R"(factorydebugv4/log/)";
    QDir dir(basePath);
    dir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    dir.setSorting(QDir::Name | QDir::Reversed);

    QString latestFolder;
    for (const QFileInfo &info : dir.entryInfoList()) {
        if (info.fileName().startsWith("2")) {
            latestFolder = info.absoluteFilePath();
            break;
        }
    }
    if (latestFolder.isEmpty())
        return;

    QString logFilePath = latestFolder + "/system/system.log";

    QFile file(logFilePath);
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        showlog("请先拉日志" + logFilePath);
        return;
    }

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
void factory_analyzer::updateForwardTable() {
    if (!adb)
        return;

    qDebug().noquote() << "ADB updateForwardTable";

    // 使用长连接 shell
    adb->sendCommand(
        "duss_shell stat --show forward",
        [this](const QString &output, qint64 elapsed) {
        qDebug() << "[ADB] updateForwardTable elapsed:" << elapsed << "ms";

        if (output == "ADB不可用") {
            QMessageBox::warning(this, "错误", "设备未连接或ADB不可用！");
            return;
        }

        // 清空表格
        table->setRowCount(0);

        // 按行解析输出
        QStringList lines =
            output.split(QRegExp("[\r\n]+"), Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            if (line.contains("----msg:")) {
                parseAndAddLine(line);
            }
        }
        },
        5000); // 5 秒超时，可根据情况调整
}

QString translateId(const QString &raw) { // 在类里或者函数外定义映射表
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

    QString id = parts[1];                   // 取后半部分
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

        showlog("[NO MATCH 2]" + line);
        return;
    }

    QString msgId = m.captured(1);
    // QString from = m.captured(2);
    // QString to = m.captured(3);
    // 使用映射表
    // 使用
    QString fromRaw = m.captured(2); // 例如 "0x0000:0x0802"
    QString toRaw = m.captured(3);   // 例如 "0x0000:0x0200"

    QString fromName = translateId(fromRaw);
    QString toName = translateId(toRaw);

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
    showlog("开始获取duss shell 通信链路 ");
    updateForwardTable(); // 启动时立即更新
}
void factory_analyzer::closeEvent(QCloseEvent *) {
    SETTINGS.setValue("Window/Size", this->size());
    ddr_press = false;
}
void factory_analyzer::runCmd(const QString &cmd) {
    showlog("[RUN] " + cmd);

    QProcess p;
    p.start(cmd);

    // 不要阻塞 UI
    while (!p.waitForFinished(100)) {
        QCoreApplication::processEvents();
        if (ddr_press == false) {
            showlog("退出等待");
            return;
        }
    }

    QString output = p.readAllStandardOutput();
    QString err = p.readAllStandardError();

    if (!output.isEmpty())
        showlog(output);
    if (!err.isEmpty())
        showlog("[ERR] " + err);
}

void factory_analyzer::on_pushButton_10_clicked() {
    const int LOOP = 1;
    ddr_press = true;
    showlog("\n===== START PRESS REBOOT TEST =====");

    for (int i = 1; i <= LOOP; i++) {
        showlog(QString(">>> ROUND %1 / %2").arg(i).arg(LOOP));

        execAdbBlocking("wait-for-device", 30000);

        if (ddr_press == false) {
            showlog("退出PRESS REBOOT");
            return;
        }

        // 1. 重启
        execAdb(
            "shell reboot",
            [this](const QString &output, qint64 elapsed) {
                qDebug() << "Elapsed:" << elapsed << "ms\n" << output;
                showlog(output);
        },
            30000);

        if (ddr_press == false) {
            showlog("退出PRESS REBOOT");
            return;
        }
        // 2. 等待设备上线

        execAdbBlocking("wait-for-device", 30000);

        showlog(QString(">>> DONE REBOOT ROUND %1").arg(i));
        showlog("-----------------------------");
        if (ddr_press == false) {
            showlog("退出PRESS REBOOT");
            return;
        }
    }

    execAdbBlocking("wait-for-device", 30000);

    if (ddr_press == false) {
        showlog("退出PRESS REBOOT");
        return;
    }
    showlog("\n===== REBOOT PRESS FINISHED =====");
    showlog("===== START FINAL STEPS =====");

    // 3. 修改关机配置
    QString cmd =
        "shell simulate_device -s DevicePowerUserIdleControlAutoShutdownTime 0";

    execAdb(
        cmd,
        [this](const QString &output, qint64 elapsed) {
        qDebug() << "Elapsed:" << elapsed << "ms\n" << output;
        showlog(output);
        },
        30000);

    showlog(">>> Set AutoShutdownTime = 0");

    on_pushButton_11_clicked();

    showlog("===== PASS =====");
}

void factory_analyzer::on_pushButton_11_clicked() {

    QString cmd = "shell test_mp_stage.sh erase && test_mp_stage.sh ddr_press "
                  "1800 &&reboot ";

    execAdb(
        cmd,
        [this](const QString &output, qint64 elapsed) {
        qDebug() << "Elapsed:" << elapsed << "ms\n" << output;
        showlog(output);
        },
        30000);
}

void factory_analyzer::on_pushButton_12_clicked() { ddr_press = false; }

void factory_analyzer::on_pushButton_15_clicked() {
    adb->sendCommand("reboot", [](const QString &output, qint64 elapsed) {
        qDebug() << "Command output:" << output;
        qDebug() << "Elapsed:" << elapsed << "ms";
    });
}
void factory_analyzer::displayCmdline(QTableWidget *table,
                                      const QString &cmdline) {
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
        if (key.contains("mp_state", Qt::CaseInsensitive) ||
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
QString parseJsonLine(const QString &line) {
    QString value = line;
    value = value.trimmed();      // 去掉首尾空格/换行
    int idx = value.indexOf(':'); // 找冒号
    if (idx != -1) {
        value = value.mid(idx + 1); // 取冒号后面部分
    }
    value = value.remove('"').remove(',').trimmed(); // 去掉引号和逗号
    return value;
}

void factory_analyzer::on_pushButton_16_clicked() {
    adb->sendCommand("cat /proc/cmdline", [this](const QString &output,
                                                 qint64 elapsed) {
        qDebug() << "Command output:" << output;
        qDebug() << "Elapsed:" << elapsed << "ms";

        // 去掉多余换行和空格
        QString cmdline = output;
        cmdline = cmdline.simplified(); // 去掉多余空格/换行，连续空白合并为一个空格

        displayCmdline(ui->tableWidget_2, cmdline);
    });

    // 硬件型号
    adb->sendCommand(R"(cat /system/etc/dji.json | grep "hw_str")",
                     [this](const QString &output, qint64) {
                         QString hw = parseJsonLine(output);
                         showlog("设备名: " + hw); // AC206 AC
                     });

    // 固件版本
    adb->sendCommand(R"(cat /blackbox/system/ver_info.txt | grep Version)",
                     [this](const QString &output, qint64) {
                         QString ver = parseJsonLine(output);
                         showlog("固件版本: " + ver); // v00.09.11.09
                     });
}

void factory_analyzer::updateQualcommComStatus() {
    if (!usbStatusLabel || !shellMonitor)
        return;

    shellMonitor->sendCommand(
        "wmic path Win32_PnPEntity where \"Name like '%Qualcomm%COM%'\" get Name",
        [this](const QString &out, qint64 /*t*/) {
            QRegularExpression comRx(R"(COM\d+)");
            QString foundCom;

            const QStringList lines = out.split('\n');
            for (const QString &lineRaw : lines) {
                QString line = lineRaw.trimmed();
                if (line.isEmpty())
                    continue;
                if (line.contains("Name"))
                    continue;

                QRegularExpressionMatch m = comRx.match(line);
                if (m.hasMatch()) {
                    foundCom = m.captured(0);
                    break;
                }
            }

            // UI 更新（已经在主线程）
            if (!foundCom.isEmpty()) {
                usbStatusLabel->setText(
                    QString("Qualcomm COM: <font color='green'>%1</font>")
                        .arg(foundCom));
                qDebug() << "Qualcomm COM: OK";
            } else {
                usbStatusLabel->setText(
                    "Qualcomm COM: <font color='red'>NONE</font>");
                // qDebug() << "Qualcomm COM:NONE";
            }
        },
        3000); // 要花费2秒后面修一下
}

void factory_analyzer::on_pushButton_17_clicked() {
    adb->sendCommand(
        R"(for f in /dev/thermal/*/temp* /dev/thermal/*/*/temp*; do
                v=$(cat "$f" 2>/dev/null)
                [ -n "$v" ] && echo "$f=$v"
           done)",
        [this](const QString &output, qint64 elapsed) {
            qDebug() << "Command output:" << output;
            qDebug() << "Elapsed:" << elapsed << "ms";
            showlog("=== Thermal Info ===");

            QStringList lines = output.split('\n', Qt::SkipEmptyParts);

            for (const QString &line : lines) {
                // line: /dev/thermal/ntc/OW002=32000
                QStringList parts = line.split('=');
                if (parts.size() != 2)
                    continue;

                QString node = parts[0];
                QString valStr = parts[1].trimmed();

                bool ok = false;
                int val = valStr.toInt(&ok);
                if (!ok)
                    continue;

                QString tempStr;
                if (val > 10000) {
                    tempStr = QString::number(val / 1000.0, 'f', 1) + " °C";
                } else if (val > 1000) {
                    tempStr = QString::number(val / 100.0, 'f', 1) + " °C";
                } else if (val > 100) {
                    tempStr = QString::number(val / 10.0, 'f', 1) + " °C";
                } else {
                    tempStr = QString::number(val) + " °C";
                }

                showlog(QString("%1 : %2").arg(node, tempStr));
            }

            showlog("====================");
        },
        3000);
}

void factory_analyzer::updateBatteryLevel() {
    // 使用 adb shell 读取电池容量
    adb->sendCommand("cat /sys/class/power_supply/battery/capacity",
                     [this](const QString &output, qint64) {
                         // 去掉换行和空格
                         QString str = output.trimmed();
                         bool ok = false;
                         int level = str.toInt(&ok);
                         if (ok) {
                             ui->progressBar->setValue(level);
                             // qDebug() << "Battery level:" << level;
                         } else {
                             qDebug() << "Failed to parse battery level:" << output;
                         }
                     });
}

void factory_analyzer::on_pushButton_18_clicked() {
    adb->sendCommand(" ps -A| grep dji_",
                     [this](const QString &output, qint64 elapsed) {
                         qDebug() << "Elapsed:" << elapsed << "ms";
                         showlog(output);
                     });
}

void factory_analyzer::on_lineEdit_returnPressed() {
    QString cmd = ui->lineEdit->text().trimmed();
    if (cmd.isEmpty())
        return;

    // 添加到历史
    commandHistory.append(cmd);
    historyIndex = commandHistory.size(); // 指向最后一条

  showlog("> " + cmd);
    // 使用 Qadb 封装的 sendCommand
  adb->sendCommand(cmd, [this](const QString &output, qint64 elapsed) {
      // 输出命令结果
      showlog(output);
  });

  ui->lineEdit->clear(); // 清空输入框
}

bool factory_analyzer::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->lineEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            if (historyIndex > 0) {
                historyIndex--;
                ui->lineEdit->setText(commandHistory.at(historyIndex));
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_Down) {
            if (historyIndex < commandHistory.size() - 1) {
                historyIndex++;
                ui->lineEdit->setText(commandHistory.at(historyIndex));
            } else {
                historyIndex = commandHistory.size();
                ui->lineEdit->clear();
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void factory_analyzer::on_pushButton_19_clicked() {
    if (treeExpanded) {
        ui->treeView->collapseAll(); // 收起
        treeExpanded = false;
        ui->pushButton_19->setText("展开全部");
    } else {
        ui->treeView->expandAll(); // 展开
        treeExpanded = true;
        ui->pushButton_19->setText("收起全部");
    }
}

void factory_analyzer::on_pushButton_20_clicked() {
    adb->sendCommand(" ps -A| grep plt_",
                     [this](const QString &output, qint64 elapsed) {
                         qDebug() << "Elapsed:" << elapsed << "ms";
                         showlog(output);
                     });
}

void factory_analyzer::on_pushButton_21_clicked() {
    adb->sendCommand("df -h", [this](const QString &output, qint64 elapsed) {
        qDebug() << "Elapsed:" << elapsed << "ms";
        showlog(output);
    });
}

void factory_analyzer::on_pushButton_22_clicked() {
    execAdb(" reboot edl -f", [this](const QString &output, qint64 elapsed) {
        qDebug() << "Elapsed:" << elapsed << "ms" << output;
        showlog("完成");
    });
}

void factory_analyzer::on_pushButton_23_clicked() {}
void deleteBuildFiles(const QString &dirPath) {
    QDir dir(dirPath);
    if (!dir.exists()) {
        qDebug() << "目录不存在:" << dirPath;
        return;
    }

    // 设置过滤条件：文件 + 指定后缀
    QStringList filters;
    filters << "*.pdb" << "*.map" << "*.exe";
    QFileInfoList fileList =
        dir.entryInfoList(filters, QDir::Files | QDir::NoSymLinks);

    for (const QFileInfo &fileInfo : fileList) {
        QString filePath = fileInfo.absoluteFilePath();
        if (QFile::remove(filePath)) {
            qDebug() << "已删除:" << filePath;
        } else {
            qDebug() << "删除失败:" << filePath;
        }
    }
}

void factory_analyzer::on_pushButton_24_clicked() {
    QString basePath = R"(所有log/)";
    deleteDirContent(basePath);
    QString buildDir = R"(./)";
    deleteBuildFiles(buildDir);
}

void factory_analyzer::on_pushButton_25_clicked()
{

    bulk->get_dev_ver();
}


void factory_analyzer::on_pushButton_26_clicked()
{
    adb->sendCommand("test_ufs_value.sh write 1", [this](const QString &output, qint64 elapsed) {
        qDebug() << "Elapsed:" << elapsed << "ms";
        showlog(output);
    });
    adb->sendCommand("test_ufs_value.sh read 1", [this](const QString &output, qint64 elapsed) {
        qDebug() << "Elapsed:" << elapsed << "ms";
        showlog(output);
    });
}


void factory_analyzer::on_lineEdit_2_returnPressed()
{
    bulk->set_amt_test(ui->lineEdit_2->text());
}


void factory_analyzer::on_pushButton_27_clicked()
{

     bulk->set_amt_clean_flag();
}


void factory_analyzer::on_pushButton_28_clicked()
{
     bulk->set_amt_check_clean_flag();

}

