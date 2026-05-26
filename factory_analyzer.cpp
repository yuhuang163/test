#include "factory_analyzer.h"
#include "agreement/qmomcozy/qproduct.h"
#include "qcustomplot.h"
#include "ui_factory_analyzer.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QProcess>
#include <QRegExp>
#include <QUrl>
#include <functional>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWidget>
#include <QQmlContext>

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

// 调试按键
void factory_analyzer::on_pushButton_14_clicked() {}


factory_analyzer::factory_analyzer(QWidget *parent)
    : QMainWindow(parent) ,bulk(new QBulk), log(new Qlog), adb(new Qadb),shell(new Qshell),
    shellMonitor(new Qshell), productSerialPort(new QSerialPort(this)),
    product(new Qproduct(productSerialPort, this)),ui(new Ui::factory_analyzer) {
    ui->setupUi(this);
    setAcceptDrops(true);
    adb->start();
    shell->start();
    shellMonitor->start();

    updateMainStyle(":/stytle/qss/Ubuntu.qss");


    QWidget *page11 = ui->tabWidget->widget(10);



    // 创建 QQuickWidget
    QQuickWidget *quickWidget = new QQuickWidget(page11);
    quickWidget->setResizeMode(QQuickWidget::SizeRootObjectToView);
    quickWidget->rootContext()->setContextProperty("factory_analyzer", this);
    quickWidget->setSource(QUrl("qrc:/new_production.qml"));

    // 让它铺满整个页面
    QVBoxLayout *layout = new QVBoxLayout(page11);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(quickWidget);

    page11->setLayout(layout);


    //  // 在你的窗口中创建 MyOpenGLWidget 对象
    //  MyOpenGLWidget *openGLWidget = new MyOpenGLWidget(this);

    //  openGLWidget->setGeometry(10, 10, 400, 400);  // 设置小部件的位置和大小
    //  openGLWidget->show();

    // QWidget *page9 =ui-> tabWidget->widget(9);

    // QVBoxLayout *layout = new QVBoxLayout(page9);  // 创建布局管理器
    // layout->addWidget(openGLWidget);  // 添加 MyOpenGLWidget 到第9页

    // page9->setLayout(layout);  // 设置新的布局

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

    connect(adb_check_timer, &QTimer::timeout, this, &factory_analyzer::updateAdbStatus);
    adb_check_timer->start(1000); // 1 秒刷新一次

    adbStatusLabel = new QLabel("ADB连接：<font color='red'>wait</font>");
    ui->statusbar->addPermanentWidget(adbStatusLabel);

    usbStatusLabel = new QLabel("usb状态：<font color='red'>wait</font>");
    ui->statusbar->addPermanentWidget(usbStatusLabel);

    bulkStatusLabel = new QLabel("bulk连接：<font color='red'>wait</font>");
    ui->statusbar->addPermanentWidget(bulkStatusLabel);

    uartStatusLabel = new QLabel("uart连接：<font color='red'>wait</font>");
    ui->statusbar->addPermanentWidget(uartStatusLabel);



    json_treeModel = new QStandardItemModel(this);
    json_tableModel = new QStandardItemModel(this);

    ui->treeView_2->setModel(json_treeModel);
    ui->tableView_2->setModel(json_tableModel);

    json_treeModel->setHorizontalHeaderLabels(QStringList() << "json树");

    connect(ui->treeView_2,
            &QTreeView::clicked,
            this,
            &factory_analyzer::json_onTreeClicked);




    // Tree model
    treeModel = new QStandardItemModel(this);
    treeModel->setHorizontalHeaderLabels({"设备树"});
    ui->treeView->setModel(treeModel);



    // File model
    fileModel = new QStandardItemModel(this);
    fileModel->setHorizontalHeaderLabels(
        {"名字", "大小", "类型", "日期", "权限"});
    ui->tableView->setModel(fileModel);

    ui->treeView->setContextMenuPolicy(Qt::CustomContextMenu);
    ui->tableView->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(ui->treeView, &QWidget::customContextMenuRequested, this,
            &::factory_analyzer::onTreeViewContextMenu);
    connect(ui->tableView, &QWidget::customContextMenuRequested, this,
            &factory_analyzer::onTableViewContextMenu);
        scanSerialPortsTimer->start(1000);  // 每秒刷新一次
    connect(this->productSerialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
            SLOT(handleProductSerialPortError(QSerialPort::SerialPortError)));
    connect(scanSerialPortsTimer, SIGNAL(timeout()), this, SLOT(scanSerialPorts()));
        connect(this, SIGNAL(refreshProductSerialPortState(int)), this, SLOT(refreshProductUartState(int)));
    connect(productSerialPort, &QSerialPort::readyRead, this, [=]() {
        productSerialPortTimer->start(10);                          // 设置100毫秒的延时
        productSerialPortBuf.append(productSerialPort->readAll());  // 将读到的数据放入缓冲区
    });

    ui->lineEdit->installEventFilter(this);

    execAdb(
        "--version",
        [this](const QString &output, qint64 elapsed) {
        qDebug() << "Elapsed:" << elapsed << "ms\n" << output;
        showlog(output);
        },
        3000);

    setupUSB();

    for (const QString &text : items) {
        ui->comboBox->addItem(text);
    }

    initTimeline();

    createTestFunctions();
    conFiglayout = qobject_cast<QVBoxLayout *>(ui->config_areas);

    // 获取QGridLayout，而不是QVBoxLayout
    canUselayout = qobject_cast<QGridLayout *>(ui->use_areas);

    // 定义行列数
    size_t colCount = 3; // 例如：3列

    canUserCol = colCount;
    // 计算 canUserRow 并向上取整
    canUserRow = (testFunctions.size() + colCount - 1) / colCount;

    for (int i = 0; i < testFunctions.size(); ++i) {
        // 创建复选框，使用 NamedFunction 结构体中的名称
        DraggableCheckBox *checkBox =
            new DraggableCheckBox(testFunctions[i].name, i, this);
        checkBoxes.append(checkBox); // 添加到复选框列表

        // 计算行和列的位置
        int Row = i / colCount;
        int Col = i % colCount;

        // 将复选框添加到网格布局的指定位置
        canUselayout->addWidget(checkBox, Row, Col);
        singleCheckBoxHeight = checkBox->sizeHint().height();
    }
    for (int r = 0; r < canUserRow; ++r) {
        for (int c = 0; c < canUserCol; ++c) {
            QFrame *cell = new QFrame(this);
            cell->setFrameShape(QFrame::Box);
            cell->setLineWidth(1);

            QVBoxLayout *cellLayout = new QVBoxLayout(cell);
            cellLayout->setContentsMargins(2, 2, 2, 2);

            canUselayout->addWidget(cell, r, c);
            cell->setAttribute(Qt::WA_TransparentForMouseEvents);
        }
    }

    // 设置网格布局的行间距和列间距
    canUselayout->setVerticalSpacing(ROW_SPACING);      // 设置行间距为 10 像素
    canUselayout->setHorizontalSpacing(COLUMN_SPACING); // 设置列间距为 10 像素
    canUselayout->setContentsMargins(MARGIN, MARGIN, MARGIN,
                                     MARGIN); // 边距设置为 10 像素

    setAcceptDrops(true); // 允许接收拖放操作
    testResultTableInit();

    reorderCheckBoxes();

    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; "
                                   "color: black;  border-radius: "
                                   "10px; padding: 10px; text-align: center; ");

    on_tabWidget_currentChanged(ui->tabWidget->currentIndex());




    searchEdit = new QLineEdit(ui->msgEdit);
    searchEdit->setGeometry(20, 20, 200, 30);
    searchEdit->setPlaceholderText("搜索...");
    searchEdit->setStyleSheet(
        "QLineEdit { background:#ffffcc; color:black; }"
        );
    searchEdit->hide();
    searchEdit->setFocusPolicy(Qt::StrongFocus);

    // 只保留 Ctrl+F
    QShortcut *shortcutFind = new QShortcut(QKeySequence::Find, this);
    connect(shortcutFind, &QShortcut::activated, this, [=]() {
        searchEdit->show();
        searchEdit->raise();
        searchEdit->setFocus();
    });

    connect(searchEdit, &QLineEdit::textChanged, this,
            [=](const QString &text) {

                if (text.isEmpty()) {
                    ui->msgEdit->setExtraSelections({});
                    return;
                }

                // ⭐ 关键：每次搜索都从文档开头开始
                QTextCursor cursor(ui->msgEdit->document());
                cursor.movePosition(QTextCursor::Start);
                ui->msgEdit->setTextCursor(cursor);

                // 定位第一个
                ui->msgEdit->find(text);

                // 高亮所有
                highlightAll(ui->msgEdit, text);
            });

    // connect(searchEdit, &QLineEdit::returnPressed, this, [=]() {

    //     QString text = searchEdit->text();
    //     if (text.isEmpty())
    //         return;

    //     // 从当前光标继续找
    //     if (!ui->msgEdit->find(text)) {
    //         // 到末尾了，从头再来
    //         QTextCursor cursor(ui->msgEdit->document());
    //         cursor.movePosition(QTextCursor::Start);
    //         ui->msgEdit->setTextCursor(cursor);
    //         ui->msgEdit->find(text);
    //     }
    // });


    searchEdit->installEventFilter(this);


}
void factory_analyzer::json_loadFile(const QString &path)
{
    QFile file(path);

    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);

    json_rootObject = doc.object();

    json_treeModel->clear();
    json_treeModel->setHorizontalHeaderLabels(QStringList() << "JSON Modules");

    json_buildTree(json_rootObject, json_treeModel->invisibleRootItem());
}
void factory_analyzer::json_loadFile_mechine()
{
    adb->sendCommand(R"(cat /system/etc/dji.json)",
                     [this](const QString &output, qint64) {

                         QByteArray data = output.toUtf8();

                         QJsonParseError err;
                         QJsonDocument doc = QJsonDocument::fromJson(data, &err);

                         if (err.error != QJsonParseError::NoError) {
                             qDebug() << "JSON parse error:" << err.errorString();
                             return;
                         }

                         json_rootObject = doc.object();

                         json_treeModel->clear();
                         json_treeModel->setHorizontalHeaderLabels(QStringList() << "JSON Modules");

                         json_buildTree(json_rootObject, json_treeModel->invisibleRootItem());
                     });
}

void factory_analyzer::json_showObject(const QJsonObject &obj)
{
    json_tableModel->clear();

    json_tableModel->setHorizontalHeaderLabels(
        QStringList() << "Key" << "Value");

    for (QString key : obj.keys())
    {
        QJsonValue val = obj[key];

        QList<QStandardItem*> row;

        row << new QStandardItem(key);

        if (val.isString())
            row << new QStandardItem(val.toString());

        else if (val.isDouble())
            row << new QStandardItem(QString::number(val.toDouble()));

        else if (val.isBool())
            row << new QStandardItem(val.toBool() ? "true" : "false");

        else
            row << new QStandardItem("[object]");

        json_tableModel->appendRow(row);
    }
}
void factory_analyzer::json_showArray(const QJsonArray &arr)
{
    json_tableModel->clear();

    if (arr.isEmpty())
        return;

    QJsonObject first = arr.first().toObject();

    QStringList headers;

    for (QString key : first.keys())
        headers << key;

    json_tableModel->setHorizontalHeaderLabels(headers);

    for (auto v : arr)
    {
        QJsonObject obj = v.toObject();

        QList<QStandardItem*> row;

        for (QString key : headers)
        {
            row << new QStandardItem(obj[key].toVariant().toString());
        }

        json_tableModel->appendRow(row);
    }
}

void factory_analyzer::json_onTreeClicked(const QModelIndex &index)
{
    QStandardItem *item = json_treeModel->itemFromIndex(index);

    if (!item)
        return;

    QVariant data = item->data(Qt::UserRole);

    if (!data.isValid())
        return;

    QJsonValue val = data.value<QJsonValue>();

    qDebug() << "val1:" << val;

    if (val.isObject())
    {
        QJsonObject obj = val.toObject();
   qDebug() << "obj:" << obj;
        // 判断是不是 route table
        if (!obj.isEmpty())
        {
            QString firstKey = obj.keys().first();

            QJsonValue firstVal = obj.value(firstKey);

            if (firstVal.isObject())
            {
                QJsonObject route = firstVal.toObject();

                // DJI route table 特征
                if (route.contains("target") && route.contains("channel"))
                {
                    json_showRouteTable(obj);
                       qDebug() << "obj2222:" << obj;
                    return;
                }
            }
        }

        // 普通 JSON
        json_showObject(obj);
    }
    else if (val.isArray())
    {
           qDebug() << "val2:" << val;
        json_showArray(val.toArray());
    }  else
    {
        // 处理 string / number / bool
        json_tableModel->clear();
        json_tableModel->setHorizontalHeaderLabels({"Value"});

        QList<QStandardItem*> row;

        if (val.isDouble())
            row << new QStandardItem(QString::number(val.toDouble()));
        else if (val.isString())
            row << new QStandardItem(val.toString());
        else if (val.isBool())
            row << new QStandardItem(val.toBool() ? "true" : "false");
        else
            row << new QStandardItem("null");

        json_tableModel->appendRow(row);
    }
}

void factory_analyzer::json_showRouteTable(const QJsonObject &obj)
{
    json_tableModel->clear();

    QStringList headers;

    headers << "route_id"
            << "target"
            << "protocol"
            << "channel"
            << "index"
            << "distance"
            << "status";

    json_tableModel->setHorizontalHeaderLabels(headers);

    for (QString route_id : obj.keys())
    {
        QJsonObject route = obj.value(route_id).toObject();
        // 先过滤无效数据
        if (!route.contains("target") || !route.contains("channel"))
            continue;
        QList<QStandardItem*> row;

        row << new QStandardItem(route_id);
        row << new QStandardItem(route["target"].toString());
        row << new QStandardItem(route["protocol"].toString());
        row << new QStandardItem(route["channel"].toString());
        row << new QStandardItem(QString::number(route["index"].toInt()));
        row << new QStandardItem(QString::number(route["distance"].toInt()));
        row << new QStandardItem(QString::number(route["status"].toInt()));

        json_tableModel->appendRow(row);
    }

    ui->tableView_2->resizeColumnsToContents();
}


void factory_analyzer::json_buildTree(const QJsonObject &obj, QStandardItem *parent)
{
    for (const QString &key : obj.keys())
    {
        QJsonValue value = obj.value(key);   // 关键：不要用 obj[key]

        QStandardItem *item = new QStandardItem(key);

        item->setData(QVariant::fromValue(value), Qt::UserRole);

        parent->appendRow(item);

        if (value.isObject())
        {
            json_buildTree(value.toObject(), item);
        }

        if (value.isArray())
        {
            QJsonArray arr = value.toArray();

            for (int i = 0; i < arr.size(); i++)
            {
                QJsonValue v = arr.at(i);

                if (v.isObject())
                {
                    QStandardItem *child = new QStandardItem(QString("[%1]").arg(i));

                    child->setData(QVariant::fromValue(v), Qt::UserRole);

                    item->appendRow(child);

                    json_buildTree(v.toObject(), child);
                }
            }
        }
    }
}


void factory_analyzer::updateMainStyle(QString style) {
    // QSS文件初始化界面样式
    QString stylesheet;


    QFile qss(style);


    if (qss.open(QFile::ReadOnly)) {
        qDebug() << "qss load";
        QTextStream filetext(&qss);
        stylesheet = filetext.readAll();
        this->setStyleSheet(stylesheet);
        qss.close();
    } else {
        qDebug() << "qss not load";
        qss.setFileName("/qss/" + style);
        if (qss.open(QFile::ReadOnly)) {
            qDebug() << "qss load";
            QTextStream filetext(&qss);
            stylesheet = filetext.readAll();
            this->setStyleSheet(stylesheet);
            qss.close();
        }
    }
}
void factory_analyzer::readProductSerialPortData() {
    productSerialPortTimer->stop();              // 关闭定时器
    QByteArray dataTemp = productSerialPortBuf;  // 读取缓冲区数据

    // qDebug() << "product data len : " << dataTemp.size();
    if (product)
        product->parseCmd(dataTemp);

    if (isBrushLogGet)
        log->save_brush_log(0, "new", dataTemp);
    // processReceivedData(dataTemp);


    ui->msgEdit->appendPlainText(QString::fromUtf8(dataTemp));
    productSerialPortBuf.clear();  // 清除缓冲区
}
void factory_analyzer::refreshProductUartState(int state) {
    if (state)
        showlog("product串口连接成功");
    else {
        ui->productComNameCombo->setEnabled(true);
        ui->productConnectButton->setEnabled(true);
        showlog("product串口连接断开");
    }
}

void factory_analyzer::handleProductSerialPortError(QSerialPort::SerialPortError error) {
    qDebug() << "ProductSerialPort串口问题" << error;
    if (error == QSerialPort::PermissionError) {
        closeProductSerialPort();
        // QMessageBox::warning(NULL, "警告", " 产品串口连接断开！\t\r\n");

        // showlog("蓝牙连接断开");
    }
}
void factory_analyzer::scanSerialPorts() {
    QList<QSerialPortInfo> ports = QSerialPortInfo::availablePorts();

    auto updateComboBox = [](QComboBox* comboBox, const QList<QSerialPortInfo>& ports) {
        if (!comboBox) {
            return;
        }

        // 获取当前的项目列表
        QSet<QString> currentItems;
        for (int i = 0; i < comboBox->count(); ++i) {
            currentItems.insert(comboBox->itemText(i));
        }

        // 添加新的项目
        for (const QSerialPortInfo& info : ports) {
            if (!currentItems.contains(info.portName())) {
                comboBox->addItem(info.portName());
            }
            currentItems.remove(info.portName());  // 移除已存在的项目
        }

        // 移除不存在的项目
        for (const QString& item : currentItems) {
            int index = comboBox->findText(item);
            if (index != -1) {
                comboBox->removeItem(index);
            }
        }
    };

    updateComboBox(ui->productComNameCombo, ports);
}
void factory_analyzer::openProductSerialPort() {
    if (productSerialPort->isOpen()) {
        disconnect(productSerialPortTimer, &QTimer::timeout, this,
                   &factory_analyzer::readProductSerialPortData);  // timeout执行真正的读取操作
        productSerialPort->close();
        if (product)
            product->clearProductSerialRxAccum();
    }

    // 设置串口名
    productSerialPort->setPortName(ui->productComNameCombo->currentText());
    // 设置波特率
    productSerialPort->setBaudRate(ui->lineEdit_6->text().toInt());
    // 设置数据位
    productSerialPort->setDataBits(QSerialPort::Data8);
    // 设置校验位
    productSerialPort->setParity(QSerialPort::NoParity);
    // 设置停止位
    productSerialPort->setStopBits(QSerialPort::OneStop);
    productSerialPort->setReadBufferSize(4096);

    // 设置流控制
    productSerialPort->setFlowControl(QSerialPort::NoFlowControl);  // 设置为无流控制

    if (productSerialPort->open(QIODevice::ReadWrite)) {
        // 启用RTS信号
        productSerialPort->setRequestToSend(true);
        // 启用DTR信号
        productSerialPort->setDataTerminalReady(true);

        // showlog("串口连接成功");

         emit refreshProductSerialPortState(1);
        //  at->ask_mac();//连接串口过程，复位设备写入资源复位损坏
        connect(productSerialPortTimer, &QTimer::timeout, this,
                &factory_analyzer::readProductSerialPortData);  // timeout执行真正的读取操作
    } else {
        // QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        // showlog("打开错误");
    }
}

void factory_analyzer::closeProductSerialPort() {
    // // 启用RTS信号
    // productSerialPort->setRequestToSend(false);
    // // 启用DTR信号
    // productSerialPort->setDataTerminalReady(false);
    if (productSerialPort->isOpen())
        productSerialPort->close();
    disconnect(productSerialPortTimer, &QTimer::timeout, this,
               &factory_analyzer::readProductSerialPortData);  // timeout执行真正的读取操作

    if (product)
        product->clearProductSerialRxAccum();

   emit refreshProductSerialPortState(0);

}
void factory_analyzer::addTimelineEvent(const QString &timeStr,
                                        const QString &title,
                                        const QString &detail,
                                        const QColor &color,
                                         QVector<TimelineEvent> &my_events
                                        ) {
    QDateTime dt = QDateTime::fromString(timeStr, "MM-dd hh:mm:ss.zzz");
    // if (!dt.isValid()) {
    //     qWarning() << "[Timeline] Invalid time string:" << timeStr;
    //     return;
    // }

    my_events.append({timeStr, title, detail, color, dt});

    if (!m_hasStartTime) {
        m_startTime = dt;
        m_hasStartTime = true;
    }


}
void factory_analyzer::drawTimeline(
    const QVector<TimelineEvent> &events,
    int baseY,
    const QString &axisName
    ) {
    const int EVENT_SPACING = 140;
    const int AXIS_LEN = 200000;

    // 画轴线
    my_screen->addLine(20, baseY, AXIS_LEN, baseY, QPen(Qt::black, 2));

    // 轴名称
    auto axisText = my_screen->addText(axisName);
    axisText->setDefaultTextColor(Qt::black);
    axisText->setPos(20, baseY - 30);
    axisText->setTextInteractionFlags(Qt::TextSelectableByMouse);

    int index = 0;

    for (const auto &e : events) {
        int x = static_cast<int>(index * EVENT_SPACING * m_currentScale);

        // 竖线
        my_screen->addLine(x+10, baseY - 14, x+10, baseY + 14, QPen(e.color, 2));

        // 点
        my_screen->addEllipse(
            x+10, baseY - 4, 8, 8,
            QPen(Qt::NoPen), QBrush(e.color)
            );

        // 时间
        auto timeText = my_screen->addText(e.timeStr);
        timeText->setDefaultTextColor(Qt::red);
        timeText->setPos(x + 10, baseY - 42);
        timeText->setTextInteractionFlags(
            Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard
            );

        // 标题
        auto titleText = my_screen->addText(e.title);
        titleText->setDefaultTextColor(e.color);
        titleText->setPos(x + 10, baseY - 6);
        titleText->setTextInteractionFlags(
            Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard
            );

        // 详情
        auto detailText = my_screen->addText(e.detail);
        detailText->setTextWidth(260);
        detailText->setPos(x + 10, baseY + 16);
        detailText->setTextInteractionFlags(
            Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard
            );

        index++;
    }
}
void factory_analyzer::re_drawTimeline()
{
    if (!my_screen)
        return;

    my_screen->clear();

    const int BASE_Y = 80;
    const int LANE_GAP = 180;

    drawTimeline(m_events,        BASE_Y,              "system.log");
    drawTimeline(ufei_m_events,   BASE_Y + LANE_GAP,   "全部ufei.log");

    // scene 范围
    my_screen->setSceneRect(0, 0, 20000, 400);
}




bool factory_analyzer::eventFilter(QObject *obj, QEvent *event) {
    if (obj == ui->graphicsView->viewport() && event->type() == QEvent::Wheel) {
        auto *wheel = static_cast<QWheelEvent *>(event);

        if (wheel->angleDelta().y() > 0)
            m_currentScale *= 1.15;
        else
            m_currentScale /= 1.15;

        m_currentScale = std::clamp(m_currentScale, SCALE_MIN, SCALE_MAX);

        qDebug() << "[Timeline] scale =" << m_currentScale;

        re_drawTimeline();

        return true; // ⭐ 吃掉事件，不再传
    }
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
    if (obj == searchEdit && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

        if (keyEvent->key() == Qt::Key_Return ||
            keyEvent->key() == Qt::Key_Enter) {

            // 🔥 手动触发“下一个”
            QString text = searchEdit->text();
            if (!text.isEmpty()) {
                if (!ui->msgEdit->find(text)) {
                    QTextCursor cursor(ui->msgEdit->document());
                    cursor.movePosition(QTextCursor::Start);
                    ui->msgEdit->setTextCursor(cursor);
                    ui->msgEdit->find(text);
                }
            }

            return true; // ⭐ 吃掉回车，禁止继续传播
        }
    }
    return QWidget::eventFilter(obj, event);
}

void factory_analyzer::initTimeline() {
    my_screen = new QGraphicsScene(this);
    ui->graphicsView->setScene(my_screen);

    ui->graphicsView->setDragMode(QGraphicsView::ScrollHandDrag);
    ui->graphicsView->setRenderHint(QPainter::Antialiasing);
    ui->graphicsView->resetTransform();

    ui->graphicsView->centerOn(0, BASE_Y);
    ui->graphicsView->viewport()->installEventFilter(this);
}

QString factory_analyzer::formatTime(int ms) const {
    int sec = ms / 1000;
    return QString("%1:%2")
        .arg(sec / 60, 2, 10, QChar('0'))
        .arg(sec % 60, 2, 10, QChar('0'));
}

void factory_analyzer::solveGetDjiResponse(int data,int errocode) {

    showlog(QString("[%1] 收到设备处理回应成功")
            .arg(QDateTime::currentDateTime()
                     .toString("HH:mm:ss.zzz"))
        );
    if (errocode != 0)
        showlog(QString("收到设备处理回应错误码: 0x%1")
                    .arg(errocode, 2, 16, QLatin1Char('0'))
                    .toUpper());

    getRespone = data;
}
void factory_analyzer::on_comboBox_2_activated(int index) {
    QVariantMap dev = ui->comboBox_2->itemData(index, Qt::UserRole).toMap();

    if (dev.isEmpty()) {
        qDebug() << "No device data";
        return;
    }

    uint16_t vid = dev["vid"].toUInt();
    uint16_t pid = dev["pid"].toUInt();
    int ifNum = dev["if"].toInt();

    qDebug() << "Open USB device:"
             << QString("VID=0x%1 PID=0x%2 IF=%3")
                    .arg(vid, 4, 16, QChar('0'))
                    .arg(pid, 4, 16, QChar('0'))
                    .arg(ifNum);
    bulk->closeDevice();
    if (bulk->openDevice(vid, pid, ifNum)) {
        qDebug() << "bulk 已连接";

        bulkStatusLabel->setText("bulk连接:<font color='green'>成功</font>");

        bulkreadThread->start();
        reconnectTimer->stop();
    } else {
        qDebug() << "bulk 打开失败，等待重连";
        bulkStatusLabel->setText("bulk连接:<font color='red'>失败</font>");
        reconnectTimer->start();
    }
}
void factory_analyzer::on_pushButton_54_clicked() {
    bulk->closeDevice();
    reconnectTimer->stop();
    showlog("断开bulk成功");
}
void factory_analyzer::setupUSB() {
    bulkreadThread = new QThread(this);
    reconnectTimer = new QTimer(this);
    // 1️⃣ 移动 bulk 到线程
    bulk->moveToThread(bulkreadThread);



    connect(bulk, SIGNAL(sendGetDjiResponse(int,int)), this,
            SLOT(solveGetDjiResponse(int,int)));
    connect(bulk, SIGNAL(send_bulk_data(QString)), this,
            SLOT(refreshbulkData(QString)));
    connect(bulk, SIGNAL(send2aprogress(int)), this,
            SLOT(refresh_send_bulk_Data(int)));
    connect(bulk, SIGNAL(download2aprogress(int)), this,
            SLOT(refresh_download_bulk_Data(int)));

    connect(bulk, SIGNAL(reconect()), reconnectTimer, SLOT(start()));

    connect(bulk, &QBulk::usbDeviceListReady, this,
            [this](const QSet<QBulk::UsbVidPid> &devices) {
                QSet<QString> currentItems;

        for (int i = 0; i < ui->comboBox_2->count(); ++i) {
                    currentItems.insert(ui->comboBox_2->itemText(i));
        }

        for (const auto &d : devices) {

            QString text = QString("VID=0x%1 PID=0x%2")
                               .arg(d.first, 4, 16, QChar('0'))
                               .arg(d.second, 4, 16, QChar('0'))
                               .toUpper();

            if (!currentItems.contains(text)) {
                // ✅ text 给人看
                ui->comboBox_2->addItem(text);

                // ✅ 真正的数据放到 UserRole
                QVariantMap dev;
                dev["vid"] = d.first;
                dev["pid"] = d.second;
                dev["if"] = 4; // 你现在用的是 interface 4

                ui->comboBox_2->setItemData(ui->comboBox_2->count() - 1, dev,
                                            Qt::UserRole);
            }

            currentItems.remove(text);
        }

        // 移除不存在的
        for (const QString &item : currentItems) {
            int idx = ui->comboBox_2->findText(item);
            if (idx >= 0)
                ui->comboBox_2->removeItem(idx);
        }
    });

    // 2️⃣ 线程启动 -> 阻塞读
    connect(bulkreadThread, &QThread::started, bulk, &QBulk::startRead);

    // 3️⃣ 数据到 UI
    // connect(bulk, &QBulk::readyRead, this, [this]( QByteArray &data){
    //     qDebug() << "USB RX:" << data.toHex();
    //     bulk->parseCmd( data);
    // });

    // 4️⃣ 错误处理
    connect(bulk, &QBulk::bulk_device_error, this, [this](int code, const QString &e) {
        qDebug() << "bulk error:" << e;
        bulkStatusLabel->setText("bulk连接：: <font color='red'>失败</font>");
        reconnectTimer->start();
    });

    // 5️⃣ 重连定时器
    reconnectTimer->setInterval(1000); // 每秒尝试一次
    connect(reconnectTimer, &QTimer::timeout, this,
            &factory_analyzer::tryOpenUSB);
    reconnectTimer->start();
}
void factory_analyzer::refresh_send_bulk_Data(int percent) {
    ui->progressBar_2->setValue(percent);
}
void factory_analyzer::refresh_download_bulk_Data(int percent) {
    ui->progressBar_4->setValue(percent);
}
void factory_analyzer::refreshbulkData(QString data) { showlog(data); }
void factory_analyzer::tryOpenUSB() {
    if (bulk->searchDevice()) {
        if (!bulk->isOpen()) {
            if (bulk->openDevice(0x2CA3, 0x0025, 4)) {
                qDebug() << "bulk 已连接";
                bulkStatusLabel->setText(
                    QString("bulk连接:<font color='green'>成功</font>"));

                qDebug() << "bulk连接：: OK";
                bulkreadThread->start();

                reconnectTimer->stop();
            } else {
                qDebug() << "bulk 打开失败，等待重连";
                bulkStatusLabel->setText("bulk连接:<font color='red'>失败</font>");
                reconnectTimer->start();
            }
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
            showlog("设备未连接或ADB不可用！");
            // QMessageBox::warning(this, "错误", "设备未连接或ADB不可用！");
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


    adb->startKeyMonitorAdbShell(
        "adb shell \"sh -c 'cat /dev/input/event1 & cat /dev/input/event2'\"",
        [this](const QString &keyName) {
            showlog(QString("%1 被按下").arg(keyName));
        }
        );



    // 启动 shell（不管是否成功，只是保证进程存在）

    adb->start();
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

            if (adb_status == false) {
                adbStatusLabel->setText("ADB连接：<font color='green'>成功</font>");
                adb_status = true;
                loadRoot();
                json_loadFile_mechine();
                on_pushButton_16_clicked();
            }
            // qDebug() << "[factory_analyzer] ADB连接成功";
        } else {
            adb_status = false;
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

    delete ui;
}

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
  if (!adb_status) {
      showlog("请连接设备");
      return;
  }

  showlog("开始拉日志");
  // 时间戳
  QString timestamp = QDateTime::currentDateTime().toString("yyyyMMddhhmmss");

  // 1️⃣ 一次性执行获取 device_id + ls /blackbox
  QString cmd = "deviceId=$(cat /factory_data/device_id.txt); echo "
                "DEVICE_ID=$deviceId; ls -d /blackbox/flight*/ 2>/dev/null";

  adb->sendCommand(
      cmd,
      [this, timestamp](const QString &output, qint64) {
          QStringList lines = output.split('\n', Qt::SkipEmptyParts);

          // 2️⃣ 解析 deviceId
          QString deviceId;
          QStringList flightDirs;

          ui->progressBar_3->setValue(0);
          for (const QString &line : lines) {
          if (line.startsWith("DEVICE_ID=")) {
                  deviceId = line.mid(QString("DEVICE_ID=").length()).trimmed();
          } else if (line.startsWith("/blackbox/flight")) {
              QString name = line.trimmed();
              // 去掉末尾的 '/'
              if (name.endsWith('/'))
                  name.chop(1);
              // 只保留目录名 flightXXXX
              name = QFileInfo(name).fileName();

              flightDirs << name;
          }
          }
          mf_dirTotal = flightDirs.size();
          mf_dirStep = (mf_dirTotal > 0) ? (50 / mf_dirTotal) : 0;
          mf_dirProgressIndex = 0;

        qDebug() << "deviceId" << deviceId << "lines:" << lines
                   << "output:" << output;
          // 3️⃣ 创建固定目录
        if (deviceId == "") {
            showlog("设备未写入sn，拉的日志目录不带sn");
        }

        QString logPath = "./factorydebugv4/log/" + timestamp + "_" + deviceId;
        QStringList dirs = {"system", "camera", "gui", "amt",
                            "aging_test_result"};
        QDir().mkpath(logPath);
        for (const QString &d : dirs) {
          QDir().mkpath(logPath + "/" + d);
        }

        m_dirTotal = dirs.size();
        m_dirStep = (m_dirTotal > 0) ? (50 / m_dirTotal) : 0;
        m_dirProgressIndex = 0;
        // 4️⃣ 拉固定模块
        for (const QString &d : dirs) {
          QString fullCmd =
                QString("cd %1; %3 pull /blackbox/%2; cd ../../../") // pwd;
                                  .arg(logPath, d, "../../adb/adb.exe");
          shell->sendCommand(
              fullCmd,
              [logPath, this](const QString &out, qint64 elapsed) {
                  qDebug() << "[pullDirsSequential] 完成:" << out
                           << "耗时:" << elapsed << "ms";
                  m_dirProgressIndex++;
                  int value = 0 + m_dirProgressIndex * m_dirStep;
                  value = qBound(0, value, 50);
                  ui->progressBar_3->setValue(value);
                  qDebug() << "[DIR PROGRESS]" << m_dirProgressIndex << "/"
                           << "dirs"
                           << "progress =" << value;
                  showlog("拉取完成:" + out);
              },
              100000);
        }
        qDebug() << "flightDirs" << flightDirs;
        // 5️⃣ 拉 flightXXXX 目录
        for (const QString &flight : std::as_const(flightDirs)) {
          QString localDir = logPath + "/" + flight;
          QDir().mkpath(localDir);

          QString fullCmd =
              QString("cd %1; %3 pull /blackbox/%2; cd ../../../") // pwd;
                                .arg(logPath, flight, "../../adb/adb.exe");
          // qDebug() << "flight fullCmd"  <<fullCmd;
          shell->sendCommand(
              fullCmd,
              [this](const QString &out, qint64 elapsed) {
                  mf_dirProgressIndex++;

                  int value = 50 + mf_dirProgressIndex * mf_dirStep;
                  value = qBound(50, value, 100);

                  ui->progressBar_3->setValue(value);

                  qDebug() << "[DIR PROGRESS]" << mf_dirProgressIndex << "/"
                           << "dirs"
                           << "progress =" << value;
                showlog("拉取完成:" + out);
                  if (mf_dirProgressIndex == mf_dirTotal) {
                      ui->progressBar_3->setValue(100);
                  }

                  // qDebug() << "[flightXXXX] 完成:" << out << "耗时:" << elapsed
                //          << "ms";
                // // showlog("完成:" + out);
                // ui->progressBar_3->setValue(100);
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
            showlog(QString("推送： %1 失败，请确保文件路径没有包含中文，补充信息:%2")
                        .arg(fileName)
                        .arg(output));
            qDebug().noquote() << "ADB 执行失败";
        } else {
            showlog(QString("推送 %1 成功").arg(fileName));
            if (ui->checkBox->isChecked()) {
                adb->sendCommand(
                    fileName,
                    [this](const QString &output, qint64 elapsed) {
                        qDebug() << "Command finished, elapsed:" << elapsed << "ms";
                        qDebug() << "Output:" << output;
                        showlog(output);
                    },
                    15000); // 设置长一些的超时，比如 15 秒
            }
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
            showlog(QString("推送： %1 失败，请确保文件路径没有包含中文，补充信息:%2")
                        .arg(fileName)
                        .arg(output));
            qDebug().noquote() << "ADB 执行失败";
        } else {
            showlog(QString("推送 %1 成功").arg(fileName));
            if (ui->checkBox->isChecked()) {
                adb->sendCommand(
                    fileName,
                    [this](const QString &output, qint64 elapsed) {
                        qDebug() << "Command finished, elapsed:" << elapsed << "ms";
                        qDebug() << "Output:" << output;
                        showlog(output);
                    },
                    15000); // 设置长一些的超时，比如 15 秒
            }
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
void factory_analyzer::highlightAll(QPlainTextEdit *edit, const QString &text)
{
    QList<QTextEdit::ExtraSelection> extras;
    QTextCursor cursor(edit->document());

    QColor color = QColor(255, 230, 150); // 明显的黄色

    while (!cursor.isNull() && !cursor.atEnd()) {
        cursor = edit->document()->find(text, cursor);
        if (!cursor.isNull()) {
            QTextEdit::ExtraSelection sel;
            sel.cursor = cursor;
            sel.format.setBackground(color);
            extras.append(sel);
        }
    }
    edit->setExtraSelections(extras);
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
    isTestContinue = false;
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
    QString value = line.trimmed();

    // ① 优先按 ':' 分割
    int idx = value.indexOf(':');
    if (idx != -1) {
        value = value.mid(idx + 1);
    } else {
        // ② 没有冒号 → 按连续空白分割
        // dji.build.version\t   00.09.24.33
        QStringList parts =
            value.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 2) {
            value = parts.last(); // 取最后一个最稳
        } else {
            return QString(); // 解析失败
        }
    }

    // ③ 清洗
    value.remove('"').remove(',').trimmed();

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
                qDebug() << "Qualcomm COM: "<<foundCom;
            } else {
                usbStatusLabel->setText(
                    "Qualcomm COM: <font color='red'>失败</font>");
                // qDebug() << "Qualcomm COM:wait";
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
    adb->sendCommand(
        "cat /sys/class/power_supply/battery/capacity",
        [this](const QString &output, qint64) {
            // 去掉换行和空格
            QString str = output.trimmed();
            bool ok = false;
            int level = str.toInt(&ok);
            if (ok) {
                ui->progressBar->setValue(level);
                bulk->ep_numer = 0x05;//高通
                // qDebug() << "Battery level:" << level;
            } else {
                adb->sendCommand(
                    "test_bat_info.sh soc", [this](const QString &output, qint64) {

                        QRegularExpression re(R"((\d+))");
                        auto match = re.match(output);

                        if (match.hasMatch()) {
                            int level = match.captured(1).toInt();
                            ui->progressBar->setValue(level);
                            bulk->ep_numer = 0x04;//默认走05//自研
                            // qDebug() << "Battery level:" << level;
                        } else {
                            ui->progressBar->setValue(0);
                            qDebug() << "Failed to parse battery level:" << output;

                        }
                    });
            }
        });
    if(bulk->ep_numer==0x05)
    ui->label_7->setText("识别为高通："+QString::number(bulk->ep_numer));
    else if(bulk->ep_numer==0x04)
        ui->label_7->setText("识别为自研："+QString::number(bulk->ep_numer));
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
        const QStringList requiredMounts = {"/",       "/system",       "/dev",
                                            "/system", "/data",         "/blackbox",
                                            "/cali",   "/factory_data", "/mnt"};

        for (const QString &mnt : requiredMounts) {
            if (!output.contains(QString(" %1").arg(mnt))) {
                showlog(QString("[ERR] mount point missing: %1").arg(mnt));
            }
        }
    });
}

void factory_analyzer::on_pushButton_22_clicked() {
    execAdb("reboot edl -f", [this](const QString &output, qint64 elapsed) {
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

void factory_analyzer::on_pushButton_25_clicked() {
    bulk->get_dev_ver_status();
}

void factory_analyzer::on_pushButton_26_clicked() {
    adb->sendCommand("test_ufs_value.sh write 1",
                     [this](const QString &output, qint64 elapsed) {
                         qDebug() << "Elapsed:" << elapsed << "ms";
                         showlog(output);
                     });
    adb->sendCommand("test_ufs_value.sh read 1",
                     [this](const QString &output, qint64 elapsed) {
                         qDebug() << "Elapsed:" << elapsed << "ms";
                         showlog(output);
                     });
}

void factory_analyzer::on_pushButton_27_clicked() {

    bulk->set_amt_clean_flag();
}

void factory_analyzer::on_pushButton_28_clicked() {
    bulk->set_amt_check_clean_flag();
}

void factory_analyzer::on_pushButton_29_clicked() {
    bulk->set_amt_task_get_result();
}

void factory_analyzer::on_pushButton_30_clicked() {
    bulk->set_amt_task_get_log(0);
}
//通用的脚本执行函数
void factory_analyzer::on_comboBox_activated(int index) {
    qDebug() << "[ComboBox] activated index =" << index;

    QString input = ui->comboBox->currentText().trimmed();
    qDebug() << "[ComboBox] raw text =" << input;

    if (input.isEmpty()) {
        qWarning() << "[ComboBox] empty text";
        return;
    }

    // 1️⃣ 按空格分割（支持多个空格）
    QStringList parts =
        input.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    qDebug() << "[ComboBox] split parts =" << parts;

    if (parts.isEmpty()) {
        qWarning() << "[ComboBox] no parts after split";
        return;
    }

    // 2️⃣ 第一个是脚本名
    QString cmd = parts.takeFirst();
    qDebug() << "[ComboBox] cmd =" << cmd;

    // 3️⃣ 剩余的是参数
    QByteArray param;
    if (!parts.isEmpty()) {
        param = parts.join(' ').toUtf8();
    }

    qDebug() << "[ComboBox] param(str) =" << QString::fromUtf8(param);
    qDebug() << "[ComboBox] param(hex) =" << param.toHex(' ');
    qDebug() << "[ComboBox] param len =" << param.size();

    // 4️⃣ 调用
    qDebug() << "[ComboBox] call set_amt_task_start";
    bulk->set_amt_task_start(cmd,
                             ui->lineEdit_2->text().toUInt(), // timeout
                             param                            // 参数
                             );
}

void factory_analyzer::on_pushButton_32_clicked() { bulk->set_sys_poweroff(); }

void factory_analyzer::on_pushButton_33_clicked() {
    // 硬件型号
    adb->sendCommand(R"(cat /system/etc/dji.json | grep "hw_str")",
                     [this](const QString &output, qint64) {
                         if (output.contains("hw_str", Qt::CaseInsensitive)) {
                             QString hw = parseJsonLine(output);
                             showlog("设备名: " + hw); // AC206 AC
                         }
                     });

    // 固件版本
    adb->sendCommand(R"(cat /blackbox/system/ver_info.txt | grep Version)",
                     [this](const QString &output, qint64) {
                         if (output.contains("Version", Qt::CaseInsensitive)) {
                             QString ver = parseJsonLine(output);
                             showlog("固件版本: " + ver); // v00.09.11.09
                         }
                     });
    // 固件版本
    adb->sendCommand(R"(cat /blackbox/system/ver_info.txt | grep Time_Stamp)",
                     [this](const QString &output, qint64) {
                         if (output.contains("Time_Stamp", Qt::CaseInsensitive)) {
                             QString ver = parseJsonLine(output);
                             showlog("大包日期: " + ver);
                         }
                     });
    adb->sendCommand(R"(unrd | grep dji.build.version)",
                     [this](const QString &output, qint64) {
                         if (output.contains("version", Qt::CaseInsensitive)) {
                             QString ver = parseJsonLine(output);
                             showlog("固件版本: " + ver);
                         }
                     });
    adb->sendCommand(R"(uname -a)", [this](const QString &output, qint64) {
        QString line = output.trimmed();
        QStringList parts =
            line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);

        // 最少需要这么多字段
        if (parts.size() < 6) {
            return;
        }

        // 取倒数 6 个
        QString buildTime = parts.mid(parts.size() - 7, 7).join(" ");

        showlog("小包日期: " + buildTime);
    });
}



QString translateResetReason(const QString &raw)
{

    // 兜底
    if (raw.contains("PON"))
    {
        if (raw.contains("SYSOK"))
             return "插着USB启动";

        if (raw.contains("PWR key DEB"))
            return "插电池/QS按键/shutter按键启动";

        return "上电启动";

    }

    if (raw.contains("Reset")){
        if (raw.contains("PWR key S2"))
            return "按QS按键复位";
        if (raw.contains("PSHOLD"))
            return "软件复位，reboot";
        if (raw.contains("SMPL"))
            return "突然掉电";

    }
        return "复位";

    return raw;  // 实在不认识就原样显示
}


void factory_analyzer::on_pushButton_34_clicked() {
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
        showlog("请先拉日志: " + logFilePath);
        return;
    }

    QTextStream in(&file);

    QRegularExpression timeRe(R"((\d{2}-\d{2}\s\d{2}:\d{2}:\d{2}\.\d{3}))");

    m_events.clear();


    while (!in.atEnd()) {
        QString line = in.readLine();

        // ===== ① beginning of main（无时间行）=====
        if (line.contains("beginning of main", Qt::CaseInsensitive)) {
            // qDebug() << "开机";

            QString timeStr;
            QString nextLine;

            if (!in.atEnd()) {
                nextLine = in.readLine();

                auto match = timeRe.match(nextLine);
                if (match.hasMatch()) {
                    timeStr = match.captured(1);
                }
            }

            addTimelineEvent(timeStr, "⚠ 机器启动", "beginning of main",
                             Qt::darkGreen,m_events);

            continue; // ⭐ 必须
        }

        // ===== ② 其它日志：必须有时间 =====
        auto timeMatch = timeRe.match(line);
        if (!timeMatch.hasMatch())
            continue;

        QString timeStr = timeMatch.captured(1);
        QString content = line;
        content.remove(timeRe);
        content = content.trimmed();
        // current exception temp
        if (line.contains("current exception temp", Qt::CaseInsensitive)) {
            addTimelineEvent(timeStr, "🔥 温度异常", content, Qt::red,m_events);
        } else if (line.contains("boot reason", Qt::CaseInsensitive)) {
            addTimelineEvent(timeStr, "🔌 启动原因", content,
                             QColor(255, 140, 0) // 深橙色 / DarkOrange
                             ,m_events);

            // qDebug() << "解析 line 日志:" << line;

        } else if (line.contains("[AMT]", Qt::CaseInsensitive)&&ui->checkBox_2->checkState()) {

            addTimelineEvent(timeStr, "🧪 AMT 测试内容", content,
                             QColor(160, 210, 160),m_events

                             );
        }
    }

    file.close();



    QString uefiDirPath = latestFolder + "/system";
    QDir uefiDir(uefiDirPath);

    QStringList uefiFiles = uefiDir.entryList(
        QStringList() << "uefi_*.log",
        QDir::Files
        );

    std::sort(uefiFiles.begin(), uefiFiles.end(),
              [](const QString &a, const QString &b) {
                  static QRegularExpression re("uefi_(\\d+)\\.log");

                  int na = re.match(a).captured(1).toInt();
                  int nb = re.match(b).captured(1).toInt();

                  return na > nb;   // 大 → 小
              });

    ufei_m_events.clear();

    for (const QString &fileName : uefiFiles) {
        QString uefi_logFilePath = uefiDirPath + "/" + fileName;

        QFile uefi_file(uefi_logFilePath);
        if (!uefi_file.open(QFile::ReadOnly | QFile::Text)) {
            showlog("无法打开: " + uefi_logFilePath);
            continue;
        }

        QTextStream uefi_in(&uefi_file);

        QString resetReason;
        QString timeStr;

        qDebug() << "解析 UEFI 日志:" << fileName<<uefiDirPath;

        while (!uefi_in.atEnd()) {
            QString line = uefi_in.readLine().trimmed();

            // ① Reset 原因
            if (line.contains("Reset by", Qt::CaseInsensitive) || line.contains("PON by", Qt::CaseInsensitive) ) {
                  qDebug() << "resetReason:" << line;

                ;
                resetReason = resetReason+"\n"+translateResetReason(line);
                continue;
            }

            // ② 时间戳
            if (line.contains("UTC") && line.contains(":")) {
                addTimelineEvent(
                    line,
                    fileName,
                    resetReason,
                    Qt::darkRed,
                    ufei_m_events
                    );

                resetReason.clear();
            }
        }

        uefi_file.close();
    }





    re_drawTimeline(); // 绘制所有事件


}

void factory_analyzer::on_pushButton_35_clicked() {
    adb->sendCommand(
        R"(cat /blackbox/system/system.log |grep "whitelist param_ckeck failed, param is")",
        [this](const QString &output, qint64) {
            if (output.contains("whitelist param_ckeck failed",
                                Qt::CaseInsensitive)) {
                showlog("参数白名单问题：" + output);
            } else {
                showlog("没有参数拦截问题");
            }
        });

    adb->sendCommand(
        R"(cat /blackbox/system/system.log |grep "AMT task whitelist check fail:")",
        [this](const QString &output, qint64) {
            qDebug() << output;
            if (output.contains("AMT task whitelist check fail",
                                Qt::CaseInsensitive)) {
                showlog("脚本未开白名单：" + output);
            } else {
                showlog("没有白名单问题");
            }
        });

    adb->sendCommand(
        R"(cat /blackbox/system/system.log |grep "set_test_result.sh")",
        [this](const QString &output, qint64) {
            qDebug() << output;
            if (output.contains("set_test_result.sh", Qt::CaseInsensitive)) {
                showlog("脚本运行结果：" + output);
            }
        });
}

void factory_analyzer::on_pushButton_36_clicked() {
    // ① 弹出文件选择框
    QString filePath = QFileDialog::getOpenFileName(
        this, tr("选择要发送的文件"),
        QString(),            // 默认路径（可改为 QDir::homePath()）
        tr("All Files (*.*)") // 文件过滤
        );

    // ② 用户点了取消
    if (filePath.isEmpty()) {
        qDebug() << "[SendFile] user canceled";
        return;
    }

    qDebug() << "[SendFile] select file:" << filePath;

    // ③ 调用发送接口
    bulk->set_2a_send_file_info(filePath);
}

void factory_analyzer::on_pushButton_37_clicked() {
    bulk->set_2a_send_file_info_check();
}

void factory_analyzer::on_pushButton_38_clicked() {
    bulk->set_2a_send_file_data();
}

void factory_analyzer::on_pushButton_39_clicked() {
    QString timeStr =
        QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");

    QString adbCmd =
        QString("shell \"echo '[PC_TIME] %1' >> blackbox/system/system.log\"")
                         .arg(timeStr);

    execAdb(
        adbCmd,
        [this, adbCmd](const QString &output, qint64 elapsed) {
            qDebug() << "Elapsed:" << elapsed << "ms";
            showlog(adbCmd + output);
        },
        3000);
}
void factory_analyzer::testResultTableUpdate(QVector<TestItem> &testItems) {
    if (testResultTable() == nullptr) {
        showlog("testResultTableUpdate不存在表格");
        return;
    }

    for (const auto &item : testItems) {
        // 获取当前时间戳
        // QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd
        // hh:mm:ss");

        // 插入新的一行
        int row = testResultTable()->rowCount();
        testResultTable()->insertRow(row);

        // 自动滚动到表格底部
        testResultTable()->scrollToBottom();

        // 设置每列的数据
        // testResultTable()->setItem(row, 0, new QTableWidgetItem(timestamp));
        testResultTable()->setItem(row, 0, new QTableWidgetItem(item.testItem));
        testResultTable()->setItem(row, 1, new QTableWidgetItem(item.testData));

        // 设置结果列的数据，假设结果是一个字符串
        QTableWidgetItem *resultItem = new QTableWidgetItem(item.testResult);
        QTableWidgetItem *askItem = new QTableWidgetItem(item.ask);

        // 设置失败状态的背景颜色为红色
        if (item.testResult == "失败") {
            resultItem->setBackground(QBrush(Qt::red));
        } else if (item.testResult == "通过") {
            resultItem->setBackground(QBrush(Qt::green));
        }

        testResultTable()->setItem(row, 2, resultItem);
        testResultTable()->setItem(row, 3, askItem);
    }
    // log->saveTestCsv(upperComputerVer, getMacLineEdit()->text(),
    // macInputLineEdit()->text(), testItems);

    testItems.clear();
}

void factory_analyzer::startTask() {
    if (isTestContinue) {
        ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        if (teststate == -1) {
            showlog("开始测试");
            TestResult = passValue;
            TestTime.start();
            teststate++;
        }

        if (bulk->is_open) {
            for (; teststate < conFiglayout->count();) {
                // qDebug() << "程序在跑" << teststate;
                if (canGoNext) {
                    DraggableCheckBox *checkBox = qobject_cast<DraggableCheckBox *>(
                        conFiglayout->itemAt(teststate)->widget()); // 获取复选框
                    if (checkBox->checkState()) {
                        showlog("开始测试内容：" + checkBox->text());
                        executeFunctionByName(checkBox->text()); // 执行操作

                        qDebug() << "程序在跑" << teststate << conFiglayout->count();

                        if (teststate >= 1) {
                            TestItem test;
                            test.testItem = qobject_cast<DraggableCheckBox *>(
                                                conFiglayout->itemAt(teststate - 1)->widget())
                                                ->text();
                            test.testData = "";
                            // if()
                            //      test.testResult = "失败";
                            // else
                            test.testResult = "通过";
                            test.ask = "通过";
                            testItems.append(test);
                            qDebug() << "完成测试表格添加1" + test.testItem;
                            testResultTableUpdate(testItems);
                        }
                        ++teststate;
                    }
                }

                break;
            }
        }

        if (teststate == conFiglayout->count() && teststate != 0 && canGoNext) {
            TestItem test;
            test.testItem = qobject_cast<DraggableCheckBox *>(
                                conFiglayout->itemAt(teststate - 1)->widget())
                                ->text();
            test.testData = "";
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);
            qDebug() << "完成测试表格添加2" + test.testItem;
            testResultTableUpdate(testItems);

            if (TestResult == failValue) {
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: "
                    "2px solid #FF0000; "
                    "border-radius: 10px; padding: 10px; text-align: center; ");

            } else {
                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: "
                    "2px solid #00FF00; "
                    "border-radius: 10px; padding: 10px; text-align: center;");
            }

            showlog("测试结束");
            teststate = -1;
            isTestContinue = false;
        }

        if (!sendCommand_test_result) {
            TestResult = failValue;
            if (teststate >= 1) {
                TestItem test;
                test.testItem = qobject_cast<DraggableCheckBox *>(
                                    conFiglayout->itemAt(teststate - 1)->widget())
                                    ->text();
                test.testData = "";
                test.testResult = "失败";
                test.ask = "通过";
                testItems.append(test);
                qDebug() << "完成测试表格添加1" + test.testItem;
                testResultTableUpdate(testItems);
            }
            if (TestResult == failValue) {
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: "
                    "2px solid #FF0000; "
                    "border-radius: 10px; padding: 10px; text-align: center; ");

            } else {
                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: "
                    "2px solid #00FF00; "
                    "border-radius: 10px; padding: 10px; text-align: center;");
            }
            showlog("测试失败结束");
            teststate = -1;
            isTestContinue = false;
        }
        if (teststate == 0) {
            showlog("有异常测试结束");
            teststate = -1;
            isTestContinue = false;
        }
    }
}

void factory_analyzer::on_pushButton_40_clicked() {
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; "
                                   "color: black;  border-radius: "
                                   "10px; padding: 10px; text-align: center; ");

    testResultTableInit();
    canGoNext = 1;
    isTestContinue = true;
    while (isTestContinue) {
        startTask();
        QCoreApplication::processEvents();
    }
}

void factory_analyzer::on_pushButton_41_clicked() { isTestContinue = false; }

void factory_analyzer::on_save_config_clicked() { showTestIndexes(); }



void factory_analyzer::on_pushButton_42_clicked() {
    adb->sendCommand("cat /blackbox/system/system.log | grep \"avc: denied\"",
                     [this](const QString &output, qint64 elapsed) {
                         qDebug() << "Elapsed:" << elapsed << "ms";
                         showlog(output);
                     });
}

void factory_analyzer::on_pushButton_43_clicked() {
    adb->sendCommand("setenforce 0",
                     [this](const QString &output, qint64 elapsed) {
                         qDebug() << "Elapsed:" << elapsed << "ms";
                         showlog(output);
                     });
}

void factory_analyzer::on_pushButton_44_clicked() {
    adb->sendCommand("setenforce 1",
                     [this](const QString &output, qint64 elapsed) {
                         qDebug() << "Elapsed:" << elapsed << "ms";
                         showlog(output);
                     });
}
void factory_analyzer::refreshColor1() {
    int r = ui->R1->value();
    int g = ui->G1->value();
    int b = ui->B1->value();

    QString styleSheet =
        QString("QLineEdit { background-color: rgb(%1, %2, %3); }")
                             .arg(r)
                             .arg(g)
                             .arg(b);
    ui->light1->setStyleSheet(styleSheet);
}
void factory_analyzer::on_R1_valueChanged(int value) {
    ui->label_46->setText(QString::number(value));

    refreshColor1();
}

void factory_analyzer::on_G1_valueChanged(int value) {
    ui->label_47->setText(QString::number(value));

    refreshColor1();
}

void factory_analyzer::on_B1_valueChanged(int value) {
    ui->label_48->setText(QString::number(value));

    refreshColor1();
}

void factory_analyzer::on_pushButton_45_clicked() {
    int r = ui->R1->value();
    int g = ui->G1->value();

    QString cmd = QString("echo %1 > /sys/class/leds/red/brightness\n"
                          "echo %2 > /sys/class/leds/green/brightness")
                      .arg(r)
                      .arg(g);

    adb->sendCommand(cmd, [this](const QString &output, qint64 elapsed) {
        qDebug() << "Elapsed:" << elapsed << "ms";
        showlog(output);
    });
}

void factory_analyzer::on_pushButton_47_clicked() {
    // bulk->set_2a_download_path_info("/blackbox/system");
    bulk->set_2a_download_file_info(ui->lineEdit_3->text());
    // bulk->set_2a_download_file_info("/blackbox/amt/aging_test/log.txt");
}

void factory_analyzer::on_pushButton_49_clicked() { bulk->set_device_date(); }

void factory_analyzer::on_pushButton_50_clicked() { bulk->get_device_date(); }

void factory_analyzer::on_pushButton_51_clicked() {
    bulk->set_device_restory_setting();
}

/// system/etc/dji.json
void factory_analyzer::on_pushButton_52_clicked() {
    execAdbBlocking("remount", 30000);
    execAdbBlocking("shell mount -o rw,remount /", 30000);
}

void factory_analyzer::on_pushButton_46_clicked() {
    bulk->set_write_product_status();
    showlog("成功后记得重启");
}

void factory_analyzer::on_pushButton_48_clicked() {
    bulk->get_product_active();
}

void factory_analyzer::on_pushButton_53_clicked() {
    bulk->get_product_status();
}

void factory_analyzer::on_pushButton_55_clicked() {
    bulk->get_product_dbg_misc_subcmd_count();
}

void factory_analyzer::on_pushButton_56_clicked() {
    bulk->get_Esdd_Check_Antirollback();
}

void factory_analyzer::on_pushButton_57_clicked() {
    bulk->set_Rpmb_Board(ui->lineEdit_4->text());
}

void factory_analyzer::on_pushButton_58_clicked() {
    bulk->set_Rpmb_Device(ui->lineEdit_5->text());
}

void factory_analyzer::on_pushButton_60_clicked() {
    bulk->get_current_slot();
}

void factory_analyzer::on_pushButton_59_clicked() {
    bulk->get_product_md5_result();
}

void factory_analyzer::on_pushButton_61_clicked()
{
     bulk->set_product_dbg_count();
}


void factory_analyzer::on_pushButton_62_clicked()
{
      bulk->get_Rpmb_Board();
}


void factory_analyzer::on_pushButton_63_clicked()
{
        bulk->get_Rpmb_Device();
}


void factory_analyzer::on_pushButton_64_clicked()
{
       bulk->set_sys_event_reboot();
}


void factory_analyzer::on_pushButton_65_clicked()
{
    bulk->get_root_key_status();


}


void factory_analyzer::on_tabWidget_currentChanged(int index)
{
    QWidget *current = ui->tabWidget->widget(index);

    if (current->objectName() == "tab_9") {
        ui->widget->show();
    } else {
        ui->widget->hide();
    }
}



void factory_analyzer::on_pushButton_66_clicked()
{
    execAdb("echo c > /proc/sysrq-trigger", [this](const QString &output, qint64 elapsed) {
        qDebug() << "Elapsed:" << elapsed << "ms" << output;
        showlog("完成");
    });


}


void factory_analyzer::on_pushButton_31_clicked()
{
    on_pushButton_clicked();
}




void factory_analyzer::on_productConnectButton_clicked() {
    openProductSerialPort();
    ui->productComNameCombo->setEnabled(false);
    ui->productConnectButton->setEnabled(false);
    ui->lineEdit_6->setEnabled(false);
}

void factory_analyzer::on_productDisconnectButton_clicked() {
    closeProductSerialPort();
    ui->productComNameCombo->setEnabled(true);
    ui->productConnectButton->setEnabled(true);
    ui->lineEdit_6->setEnabled(true);

}

void factory_analyzer::on_pushButton_67_clicked()
{
    adb->sendCommand("simulate_device -s DevicePowerUserIdleControlAutoShutdownTime 0",
                     [this](const QString &output, qint64 elapsed) {
                         qDebug() << "Elapsed:" << elapsed << "ms";
                         showlog(output);
                     });

}

void factory_analyzer::on_pushButton_68_clicked()
{
    QString cmd = QString("dji_sn_ops.sh board WR %1")
    .arg(ui->lineEdit_4->text());

    sendCommandWithRetry(
        std::bind(&QBulk::set_amt_task_test,
                  bulk,
                  cmd,
                  2000)
        );
}


void factory_analyzer::on_pushButton_69_clicked()
{
    QString cmd = QString("dji_sn_ops.sh device WR %1")
    .arg(ui->lineEdit_5->text());

    sendCommandWithRetry(
        std::bind(&QBulk::set_amt_task_test,
                  bulk,
                  cmd,
                  2000)
        );

}


void factory_analyzer::on_pushButton_70_clicked()
{    QString cmd = QString("eagle4_state_pro.sh");

    sendCommandWithRetry(
        std::bind(&QBulk::set_amt_task_test,
                  bulk,
                  cmd,
                  2000)
        );

showlog("成功后记得重启");
}


void factory_analyzer::on_pushButton_71_clicked()
{
    QString cmd = QString("e3t_state_pro.sh");

    sendCommandWithRetry(
        std::bind(&QBulk::set_amt_task_test,
                  bulk,
                  cmd,
                  2000)
        );

    showlog("成功后记得重启");
}


void factory_analyzer::on_pushButton_72_clicked()
{
          bulk->get_active_times();
}


void factory_analyzer::on_pushButton_73_clicked()
{
      bulk->set_wake_wifi();
}

void factory_analyzer::qmlstartTest()
{
    showlog("成功后记得重启");
}


void factory_analyzer::on_pushButton_75_clicked()
{
        json_loadFile("dji.json");

}


void factory_analyzer::on_pushButton_76_clicked()
{
        reconnectTimer->stop();
    adb_check_timer->stop();
}


void factory_analyzer::on_pushButton_74_clicked()
{
    if (json_treeExpanded) {
        ui->treeView_2->collapseAll(); // 收起
        json_treeExpanded = false;
        ui->pushButton_74->setText("展开全部");
    } else {
        ui->treeView_2->expandAll(); // 展开
        json_treeExpanded = true;
        ui->pushButton_74->setText("收起全部");
    }
}


void factory_analyzer::on_pushButton_77_clicked()
{
     json_loadFile_mechine();
}

