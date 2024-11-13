#include "qfreework.h"

#include "qpainter.h"
#include "ui_qfreework.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif
extern "C"  // 由于是C版的dll文件，在C++中引入其头文件要加extern "C" {},注意
{
#include "lib/nfc/dcrf32.h"
}
// 行间距和列间距的宏定义
#define ROW_SPACING 10
#define COLUMN_SPACING 10
#define MARGIN 10

QFreeWork::QFreeWork(int index, QWidget* parent) : ui(new Ui::QFreeWork) {
    m_index = index;
    pack.mechines = getIndex();
    upperComputerVer = FREE_VER;
    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    scanSerialPorts();  // 要搜索一下一开始
    // connect(at, SIGNAL(send_rssi(QString)), this, SLOT(refreshBleRssi(QString)));
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");

    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    ui->banding_result->setText("绑定:WAIT");
    ui->banding_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");

    connect(waittime, &QTimer::timeout, [=]() {
        isovertime = 1;
        waittime->stop();
    });

    connect(comparewaittime, &QTimer::timeout, [=]() {
        iscompareovertime = 1;
        comparewaittime->stop();
    });

    HighRssi = SETTINGS.value("WIFI/HighRssi").toDouble();
    LowRssi = SETTINGS.value("WIFI/LowRssi").toDouble();
    BleHighRssi = SETTINGS.value("BLE/HighRssi").toDouble();
    BleLowRssi = SETTINGS.value("BLE/LowRssi").toDouble();
    standbattary = SETTINGS.value("BATTARY/standbattary").toDouble();
    HighCurrent = SETTINGS.value("Current/HighCharCurrent").toDouble();
    LowCurrent = SETTINGS.value("Current/LowCharCurrent").toDouble();

    measure_wait_time = SETTINGS.value("Current/measure_wait_time").toInt();

    RssiTestTime = SETTINGS.value("BLE/RssiCount").toInt();
    // ui->wifiUserName->setText(SETTINGS.value("WIFI/Name", "请在配置文件中设置").toString());
    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "usmile123").toString());

    showlog("HighCurrent=" + QString::number(HighCurrent));
    showlog("LowCurrent=" + QString::number(LowCurrent));
    showlog("measure_wait_time=" + QString::number(measure_wait_time));

    showlog("machineNo=" + pack.machineNo);
    showlog("standbattary=" + QString::number(standbattary));
    showlog("model=" + pack.model);
    showlog("action=" + pack.test_station);
    showlog("line=" + pack.line);
    showlog("action=" + pack.action);

    if (pack.factory == "lx" || pack.factory == "jj") {
        usbBaudRate = 9600;
        connect(usb, SIGNAL(send_ammeter_data(QString)), this, SLOT(refreshAmmeterData(QString)));
    } else {
        usbBaudRate = 115200;
        ui->usbdisconnectButton->setDisabled(true);
        ui->usbconnectButton->setDisabled(true);
        ui->usbcomNameCombo->setDisabled(true);
    }

    ui->tabWidget->setTabText(0, "自由工站");
    createTestFunctions();

    // conFiglayout = qobject_cast<QVBoxLayout*>(ui->tabWidget->widget(3)->findChild<QVBoxLayout*>("config_areas"));

    // // 获取QGridLayout，而不是QVBoxLayout
    // canUselayout = qobject_cast<QGridLayout*>(ui->tabWidget->widget(3)->findChild<QGridLayout*>("use_areas"));

    conFiglayout = qobject_cast<QVBoxLayout*>(ui->config_areas);

    // 获取QGridLayout，而不是QVBoxLayout
    canUselayout = qobject_cast<QGridLayout*>(ui->use_areas);

    // 定义行列数
    int colCount = 3;  // 例如：3列

    canUserCol = colCount;
    // 计算 canUserRow 并向上取整
    canUserRow = (testFunctions.size() + colCount - 1) / colCount;

    for (int i = 0; i < testFunctions.size(); ++i) {
        // 创建复选框，使用 NamedFunction 结构体中的名称
        DraggableCheckBox* checkBox = new DraggableCheckBox(testFunctions[i].name, i, this);
        checkBoxes.append(checkBox);  // 添加到复选框列表

        // 计算行和列的位置
        int Row = i / colCount;
        int Col = i % colCount;

        // 将复选框添加到网格布局的指定位置
        canUselayout->addWidget(checkBox, Row, Col);
    }

    // 设置网格布局的行间距和列间距
    canUselayout->setVerticalSpacing(ROW_SPACING);                     // 设置行间距为 10 像素
    canUselayout->setHorizontalSpacing(COLUMN_SPACING);                // 设置列间距为 10 像素
    canUselayout->setContentsMargins(MARGIN, MARGIN, MARGIN, MARGIN);  // 边距设置为 10 像素

    setAcceptDrops(true);  // 允许接收拖放操作
    testResultTableInit();

    reorderCheckBoxes();
}
void QFreeWork::reorderCheckBoxes() {
    QVector<int> indexes = loadIndexesFromConfig();  // 从配置文件加载测试顺序

    for (int index : indexes) {
        DraggableCheckBox* checkBox = getCanUseCheckBoxByIndex(index);  // 根据索引获取对应的复选框
        if (checkBox) {
            canUselayout->removeWidget(checkBox);  // 从原布局移除复选框
            conFiglayout->addWidget(checkBox);     // 将复选框添加到目标布局
        }
    }

    qDebug() << "复选框已按照测试顺序重新排列";
}

void QFreeWork::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasText()) {  // 如果拖动的数据有文本
        // qDebug() << "拖动数据有文本Enter";
        event->acceptProposedAction();  // 接受拖动操作
    }
}

void QFreeWork::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasText()) {  // 如果拖动的数据有文本
        dragPos = event->pos();
        update();
        event->acceptProposedAction();  // 接受拖动操作
    }
}

void QFreeWork::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    // 只创建一个 QPainter 对象
    QPainter painter(this);
    // 获取 can_use 控件的几何信息
    QRect canUseLayoutArea = ui->can_use->geometry();

    singleCheckBoxHeight = (canUseLayoutArea.height() - (2 * MARGIN) + ROW_SPACING) /
                           canUserRow;  // checkBoxes[0]->height() + ROW_SPACING;

    singleCheckBoxWidth =
        (canUseLayoutArea.width() - 2 * MARGIN + COLUMN_SPACING) / canUserCol;  // checkBoxes[0]->width() +
                                                                                // COLUMN_SPACING; 绘制矩形在拖动位置
    if (!dragPos.isNull()) {
        painter.fillRect(
            QRect(dragPos, QSize(singleCheckBoxWidth - COLUMN_SPACING, singleCheckBoxHeight - ROW_SPACING)), Qt::green);
    }
}

void QFreeWork::dropEvent(QDropEvent* event) {
    qDebug() << "放下了" << event->pos();
    QPoint mouseGlobalPos = mapToGlobal(event->pos());
    qDebug() << "mouseGlobalPos Global Position:" << mouseGlobalPos;

    const QMimeData* mimeData = event->mimeData();  // 获取拖动的数据
    if (!mimeData->hasText())
        return;  // 如果没有文本数据，直接返回

    int sourceIndex = mimeData->text().toInt();
    DraggableCheckBox* sourceCheckBox = getCheckBoxByIndex(sourceIndex);
    if (!sourceCheckBox)
        sourceCheckBox = getCanUseCheckBoxByIndex(sourceIndex);  // 尝试获取can_use中的复选框

    if (!conFiglayout || !canUselayout || !sourceCheckBox)
        return;

    QRect configLayoutArea = ui->config->geometry();
    QPoint configGlobalPos = ui->config->mapToGlobal(QPoint(0, 0));
    QRect configGlobalArea(configGlobalPos.x(), configGlobalPos.y(), configLayoutArea.width(),
                           configLayoutArea.height());

    QRect canUseLayoutArea = ui->can_use->geometry();
    QPoint canUseGlobalPos = ui->can_use->mapToGlobal(QPoint(0, 0));
    QRect canUseGlobalArea(canUseGlobalPos.x(), canUseGlobalPos.y(), canUseLayoutArea.width(),
                           canUseLayoutArea.height());

    // 如果目标区域是 config 布局
    if (configGlobalArea.contains(mouseGlobalPos)) {
        qDebug() << "在config区域放置复选框";
        moveToLayout(canUselayout, conFiglayout, sourceCheckBox);

        int destIndex = getIndexAt(mouseGlobalPos);  // 位置第几个
        if (destIndex >= 0 && destIndex != conFiglayout->indexOf(sourceCheckBox)) {
            qDebug() << "在config区域调整" << destIndex;
            conFiglayout->removeWidget(sourceCheckBox);
            conFiglayout->insertWidget(destIndex, sourceCheckBox);
        }
        event->acceptProposedAction();
    }
    // 如果目标区域是 can_use 布局
    else if (canUseGlobalArea.contains(mouseGlobalPos)) {
        qDebug() << "在can_use区域放置复选框";
        int row, col;
        calculateGridPosition(mouseGlobalPos, canUseGlobalArea, row, col);
        moveToGrid(canUselayout, sourceCheckBox, row, col);
        int destIndex = getIndexAt(mouseGlobalPos);  // 位置第几个
        if (destIndex >= 0 && destIndex != canUselayout->indexOf(sourceCheckBox)) {
            qDebug() << "在can_use区域调整" << destIndex;
            canUselayout->removeWidget(sourceCheckBox);         // 从布局中移除控件
            canUselayout->addWidget(sourceCheckBox, row, col);  // 在指定行和列添加控件
        }
        event->acceptProposedAction();
    } else {
        qDebug() << "配置区域鼠标放下的位置不对"
                 << "目标" << configGlobalArea << canUseGlobalArea << "实际" << mouseGlobalPos;
    }

    dragPos = QPoint();
}

void QFreeWork::moveToLayout(QLayout* fromLayout, QLayout* toLayout, QWidget* widget) {
    if (fromLayout)
        fromLayout->removeWidget(widget);
    toLayout->addWidget(widget);
}

void QFreeWork::calculateGridPosition(const QPoint& globalPos, const QRect& area, int& row, int& col) {
    int singleCheckBoxHeight = (area.height() - 2 * MARGIN + ROW_SPACING) / canUserRow;
    int singleCheckBoxWidth = (area.width() - 2 * MARGIN + COLUMN_SPACING) / canUserCol;

    row = qBound(0, (globalPos.y() - area.y() - MARGIN) / singleCheckBoxHeight, canUselayout->rowCount() - 1);
    col = qBound(0, (globalPos.x() - area.x() - MARGIN) / singleCheckBoxWidth, canUselayout->columnCount() - 1);

    qDebug() << "计算的网格位置 - row:" << row << ", col:" << col;
}

void QFreeWork::moveToGrid(QGridLayout* layout, QWidget* widget, int row, int col) {
    if (!layout)
        return;
    if (layout->indexOf(widget) != -1)
        layout->removeWidget(widget);
    layout->addWidget(widget, row, col);
}

DraggableCheckBox* QFreeWork::getCanUseCheckBoxByIndex(int index) {
    if (!canUselayout) {
        qDebug() << "特定布局不存在";
        return nullptr;
    }
    qDebug() << "canUselayout个数" << canUselayout->count();
    // 遍历布局中的所有子项
    for (int i = 0; i < canUselayout->count(); ++i) {
        DraggableCheckBox* checkBox = qobject_cast<DraggableCheckBox*>(canUselayout->itemAt(i)->widget());

        if (checkBox && checkBox->getIndex() == index) {
            qDebug() << "拿起的索引为" << index;
            return checkBox;  // 返回匹配的复选框
        }
    }

    qDebug() << "CanUse源复选框不存在" << index << canUselayout->count();
    return nullptr;  // 如果没有找到匹配的复选框，返回空指针
}

DraggableCheckBox* QFreeWork::getCheckBoxByIndex(int index) {
    if (!conFiglayout) {
        qDebug() << "特定布局不存在";
        return nullptr;
    }

    // 遍历布局中的所有子项
    for (int i = 0; i < conFiglayout->count(); ++i) {
        DraggableCheckBox* checkBox = qobject_cast<DraggableCheckBox*>(conFiglayout->itemAt(i)->widget());

        if (checkBox && checkBox->getIndex() == index) {
            qDebug() << "拿起的索引为" << index;
            return checkBox;  // 返回匹配的复选框
        }
    }

    qDebug() << "源复选框不存在" << conFiglayout->count();
    return nullptr;  // 如果没有找到匹配的复选框，返回空指针
}

int QFreeWork::getIndexAt(const QPoint& pos) {
    qDebug() << "鼠标位置为" << pos << conFiglayout->count();
    // 遍历布局中的所有子项
    for (int i = 0; i < conFiglayout->count(); ++i) {
        // 获取当前子项的控件
        QWidget* widget = conFiglayout->itemAt(i)->widget();
        if (!widget)
            continue;  // 跳过空的子项

        // 创建新的 QRect
        QRect newconfigArea(widget->mapToGlobal(QPoint(0, 0)).x(), widget->mapToGlobal(QPoint(0, 0)).y(),
                            widget->geometry().width(), widget->geometry().height());

        // 扩大控件范围，考虑到偏移
        QRect expandedRect = newconfigArea;
        // QRect expandedRect = widget->geometry().adjusted(-5, -5, 5, 5);  // 有问题获取的
        qDebug() << "检查第" << i << "个子项，几何位置:" << expandedRect;
        // 如果位置在扩大后的范围内，则认为匹配到了该子项
        if (expandedRect.contains(pos)) {
            qDebug() << "匹配到的是这个" << i;
            return i;  // 返回该子项的位置索引
        }
    }

    qDebug() << "这个位置无匹配内容";
    return -1;  // 如果没有找到匹配的位置，返回-1
}

void QFreeWork::showTestIndexes() {
    qDebug() << "当前测试顺序为";                      // 更新复选框的索引
    for (int i = 0; i < conFiglayout->count(); ++i) {  // 遍历布局中的所有子项
        DraggableCheckBox* checkBox =
            qobject_cast<DraggableCheckBox*>(conFiglayout->itemAt(i)->widget());  // 获取复选框
        if (checkBox) {                                                           // 如果复选框存在
            qDebug() << checkBox->getIndex();                                     // 更新复选框的索引
            testIndexes.append(checkBox->getIndex());                             // 将索引添加到容器中
        }
    }
    // 将索引列表保存到文件中
    saveIndexesToConfig(testIndexes);
}
QVector<int> QFreeWork::loadIndexesFromConfig() {
    QVector<int> indexes;

    SETTINGS.beginGroup("TestOrder");         // 打开 "TestOrder" 分组
    QStringList keys = SETTINGS.childKeys();  // 获取分组中的所有键

    for (const QString& key : keys) {
        indexes.append(SETTINGS.value(key).toInt());  // 读取每个键的值并转换为整数
    }
    SETTINGS.endGroup();

    qDebug() << "测试顺序已从配置文件中加载:" << indexes;
    return indexes;
}
void QFreeWork::saveIndexesToConfig(const QVector<int>& indexes) {
    SETTINGS.beginGroup("TestOrder");  // 创建一个分组
    SETTINGS.remove("");               // 清空当前组的所有项，以免覆盖
    for (int i = 0; i < indexes.size(); ++i) {
        SETTINGS.setValue(QString::number(i), indexes[i]);  // 将每个索引存入配置文件
    }
    SETTINGS.endGroup();
}

// 获取下一个状态的函数
QFreeWork::State QFreeWork::getNextState(State currentState) {
    return static_cast<State>((static_cast<int>(currentState) + 1) % 5);
}
// 定义一个函数，用于执行具有特定名称的函数
void QFreeWork::executeFunctionByName(const QString functionName) {
    // 在 functions 向量中查找具有特定名称的函数对象
    for (const auto& namedFunction : testFunctions) {
        if (namedFunction.name == functionName) {
            // 执行找到的函数对象
            namedFunction.function();
            return;  // 执行完成后直接返回
        }
    }

    // 如果没有找到匹配的函数名，则输出错误信息
    qDebug() << "Error: Function with name " << functionName << " not found!";
}

void QFreeWork::startTask() {
    if (isTestContinue) {
        ui->test_time->display(TestTime.elapsed() / 1000);
        if (teststate == -1) {
            showlog("开始测试");
            initDate();
            at->sendMac(macAddress);  // 开始连接
            teststate++;
        }
        if (at->getConnected()) {
            for (; teststate < conFiglayout->count();) {
                // qDebug() << "程序在跑" << teststate;
                if (canGoNext) {
                    DraggableCheckBox* checkBox =
                        qobject_cast<DraggableCheckBox*>(conFiglayout->itemAt(teststate)->widget());  // 获取复选框
                    if (checkBox->checkState()) {
                        executeFunctionByName(checkBox->text());  //执行操作
                        showlog("正在测试内容：" + checkBox->text());
                        qDebug() << "程序在跑" << teststate << conFiglayout->count();

                        if (teststate >= 1) {
                            TestItem test;
                            test.testItem =
                                qobject_cast<DraggableCheckBox*>(conFiglayout->itemAt(teststate - 1)->widget())->text();
                            test.testData = "";
                            test.testResult = "通过";
                            test.ask = "通过";
                            testItems.append(test);

                            testResultTableUpdate(testItems);
                        }
                        ++teststate;
                    }
                }

                break;
            }
        }

        if (teststate == conFiglayout->count()) {
            TestItem test;
            test.testItem = qobject_cast<DraggableCheckBox*>(conFiglayout->itemAt(teststate - 1)->widget())->text();
            test.testData = "";
            test.testResult = "通过";
            test.ask = "通过";
            testItems.append(test);

            testResultTableUpdate(testItems);

            if (TestResult == failValue) {
                ui->test_result->setText("FAIL");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                    "border-radius: 10px; padding: 10px; text-align: center; ");
                pack.itemvalue = QString("|FREE_TEST:PASS|");
                pack.result = "NG";
                pack.sn = ui->getMac->text();
                pack.instruct_num = "079";
                if (ui->isusemes->checkState()) {
                    emit send_end_testPass(pack);
                }
            } else {
                ui->test_result->setText("PASS");
                ui->test_result->setStyleSheet(
                    "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                    "border-radius: 10px; padding: 10px; text-align: center;");
                pack.result = "PASS";
                pack.itemvalue = QString("|FREE_TEST:FAIL|");
                pack.sn = ui->getMac->text();
                pack.instruct_num = "079";
                if (ui->isusemes->checkState()) {
                    emit send_end_testPass(pack);
                }
            }

            qDebug() << "测试结束";
            teststate = -1;
            ui->macInput->clear();
            ui->snInput->clear();
            ui->nfc_sn->clear();
            ui->macInput->setDisabled(0);
            ui->getMac->setDisabled(0);
            waitWork(50);
            on_disconnectButton_clicked();
            emit send_end_test(getIndex());
            ui->getMac->clear();
            isTestContinue = false;
        }
    }
}

QFreeWork::~QFreeWork() { delete ui; }

void QFreeWork::refreshBaseData(FacGetDevBaseInfo data) {
    QString softwareVersion = SETTINGS.value("ProductInfo/Software_Version").toString();
    QString resourceVersion = SETTINGS.value("ProductInfo/Resource_Version").toString();
    QString Age_State = SETTINGS.value("ProductInfo/Age_State").toString();
    product = data.product_name;
    if (QString(data.product_name).compare("U7P") == 0 || QString(data.product_name).compare("U7") == 0) {
        // sku = "55040701";
        showlog("开始写入nfc数据");

        on_nfc_write_read_clicked();
    } else {
    }
    wifiMac.clear();
    for (int var = 0; var < data.wifi_mac.size; ++var) {
        wifiMac += QString::number(data.wifi_mac.bytes[var], 16);
        if (var < data.wifi_mac.size - 1)
            wifiMac += ":";
    }
    qDebug() << getIndex() << "设备的 wifiMac:" << wifiMac;

    if (data.soft_version == softwareVersion && data.res_version == resourceVersion &&
        QString::number(data.ageing_state) == Age_State) {
        showlog("软件版本正确" + QString::fromUtf8(data.soft_version));
        showlog("资源版本正确" + QString::fromUtf8(data.res_version));
        showlog("老化状态正确" + QString::number(data.ageing_state));
    } else {
        TestResult = failValue;
        showlog("状态错误");
        showlog("当前设备软件版本" + QString::fromUtf8(data.soft_version) + "配置文件版本" + softwareVersion);
        showlog("当前设备资源版本" + QString::fromUtf8(data.res_version) + "配置文件版本" + resourceVersion);
        showlog("当前设备老化状态" + QString::number(data.ageing_state) + "配置文件老化要求" + Age_State);

        // isTestContinue = false;
        // showlog("停止运行");

        // ui->macInput->clear();
        // ui->getMac->clear();
        // ui->getMac->setFocus();
    }
}

void QFreeWork::refreshBattaryData(FacDevInfo adc) {
    QString chargeStateStr;
    switch (adc.dev_info[0].value_item.battery.charge_state) {
        case 1:
            chargeStateStr = "充电状态为：<span style='color:green'>电量充满</span>";
            chargestate = "CHARGE_FULL";
            break;
        case 2:
            chargeStateStr = "充电状态为：<span style='color:orange'>正在充电</span>";
            chargestate = "CHARGING";
            break;
        case 3:
            chargeStateStr = "充电状态为：<span style='color:red'>充电断开</span>";
            chargestate = "UNCHARGED";
            break;
        case 4:
            chargeStateStr = "充电状态为：<span style='color:red'>没有电池</span>";
            chargestate = "NO_BATTER";
            break;
        default:
            chargeStateStr = "充电状态为：<span style='color:red'>未知</span>";
            chargestate = "UNKOWN_STATE";
            break;
    }
    ui->battary_state->setText(chargeStateStr);

    // 修改电量的显示样式
    QString batteryPercentStr =
        "电量为：<span style='color:blue'>" + QString::number(adc.dev_info[0].value_item.battery.percent) + "%</span>";
    ui->battary_value->setText(batteryPercentStr);

    // 修改电压的显示样式
    QString batteryVoltageStr = "电压为：<span style='color:purple'>" +
                                QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0, 'f', 3) +
                                "V</span>";
    ui->battary_voltage->setText(batteryVoltageStr);

    voltage = adc.dev_info[0].value_item.battery.voltage / 1000.0;
    // QRegularExpression regex("<span style='color:(.*?)'>(.*?)</span>");
    // QRegularExpressionMatch match = regex.match(chargeStateStr);
    // chargestate = match.captured(2);
    is_battary_test = 1;
    if (adc.dev_info[0].value_item.battery.charge_state == 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 > standbattary) {
        TestItem test;
        test.testItem = "充电测试";
        test.testData = "正在充电" + QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V";
        test.testResult = "通过";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        charageresult = "通过";
        voltageresult = "通过";
        showlog("电量和充电测试通过");
    }
    if (adc.dev_info[0].value_item.battery.charge_state != 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 > standbattary) {
        TestItem test;
        test.testItem = "充电测试";
        test.testData = "不充电" + QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V";
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        showlog("充电状态不通过");
        charageresult = "失败";
        voltageresult = "通过";
        TestResult = failValue;
    }
    if (adc.dev_info[0].value_item.battery.charge_state == 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 <= standbattary)

    {
        TestItem test;
        test.testItem = "充电测试";
        test.testData = "正在充电" + QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V";
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        showlog("电量测试不通过");
        voltageresult = "失败";
        charageresult = "通过";
        TestResult = failValue;
    }
    if (adc.dev_info[0].value_item.battery.charge_state != 2 &&
        adc.dev_info[0].value_item.battery.voltage / 1000.0 <= standbattary) {
        TestItem test;
        test.testItem = "充电测试";
        test.testData = "不充电" + QString::number(adc.dev_info[0].value_item.battery.voltage / 1000.0) + "V";
        test.testResult = "失败";
        test.ask = "通过";
        testItems.append(test);

        testResultTableUpdate(testItems);

        showlog("电量和充电测试都不通过");
        voltageresult = "失败";
        charageresult = "失败";
        TestResult = failValue;
    }
}

void QFreeWork::refreshWifiState(int state) {
    if (state) {
        // ui->WIFIStatusLabel->setText("WIFI连接：<font color='green'>成功</font>");
        //  showlog("WIFI连接成功");
        wifistate = 1;
    } else {
        //  ui->WIFIStatusLabel->setText("WIFI连接：<font color='red'>失败</font>");
        //  showlog("WIFI连接断开");
        wifistate = 0;
    }
}

void QFreeWork::refreshSn(FacDevInfo data) {
    stringsn = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    qDebug() << getIndex() << "dev_info" << data.dev_info[0].value_item.tail_sn;
    qDebug() << getIndex() << "stringsn" << stringsn;
    ui->tail_sn->setText("芯片存储的尾盖sn:" + stringsn);

    // if (stringsn == "")
    // {
    //     QMessageBox::warning(NULL, "警告", " 该设备未绑定sn！\t\r\n");
    // }
}
void QFreeWork::refreshMesState(int state) {
    if (state)
        showlog("mes登录成功");
    else
        showlog("mes登录失败");
}

void QFreeWork::getDongleWifi(QString data) {
    // 保存密码
    SETTINGS.setValue("WIFI/Password", "usmile123");
    // 保存名称，带有索引
    SETTINGS.setValue(QString("WIFI/Name%1").arg(getIndex()), data);

    ui->wifiUserName->setText(SETTINGS.value(QString("WIFI/Name%1").arg(getIndex()), "请在配置文件中设置").toString());

    ui->wifiPassword->setText(SETTINGS.value("WIFI/Password", "123445566").toString());
}
void QFreeWork::getDongleVer(QString data) { showlog("当前dongle的版本为：" + data); }

void QFreeWork::refreshBleRssi(QString data) {
    // qDebug() << data;
    ui->BLE_RSSI->setText("BLE的RSSI:" + data);
    // showlog("zzzzz"+data);
    BLE_RSSI = data;
    bool ok;
    BLE_RSSI.toInt(&ok);

    if (!ok) {
        qDebug() << "转换蓝牙rssi失败,内容为" + BLE_RSSI + "内容结束";
    } else {
        // showlog("转换成功");
        intblerssi = BLE_RSSI.toInt(&ok);
    }
}

void QFreeWork::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        //   showlog("蓝牙连接成功");
        pb->set_forbid_sleep(FacSwitch_OPEN);
        showlog("已发送禁止休眠");
    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
        // showlog("蓝牙连接断开");
    }
}

void QFreeWork::refreshDongleUartState(int state) {
    if (state)
        showlog("dongle串口连接成功");
    else {
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}
void QFreeWork::refreshUsbUartState(int state) {
    if (state)
        showlog("usb串口连接成功");
    else {
        showlog("usb串口连接断开");

        ui->usbconnectButton->setDisabled(true);
        ui->usbcomNameCombo->setDisabled(true);
    }
}
void QFreeWork::refreshAmmeterData(QString data) {
    qDebug() << getIndex() << "收到电流数据" << data;

    // 使用 toDouble() 进行转换
    bool conversionOk = false;
    double normalValue = data.toDouble(&conversionOk) / 100;

    if (conversionOk) {
        // 转换成功
        qDebug() << getIndex() << "转换后的数值：" << normalValue << "ma";
        measure_ammeter = normalValue;
        QString formattedValue = QString::number(normalValue, 'f', 4);
        qDebug() << getIndex() << "转换后的数值：" << formattedValue << "ma";
        // ui->log->appendPlainText(formattedValue+"ma");
        showlog(formattedValue + "ma");
    } else {
        // 转换失败
        qDebug() << getIndex() << "无法将字符串转换为 double 类型";
    }
}

void QFreeWork::closeEvent(QCloseEvent* event) {
    // qDebug() << getIndex() << "开始关闭";
    isTestContinue = false;
}

void QFreeWork::getWifiMsg(QString data) {
    // qDebug() << getIndex()<< "收到wifi数据为" << data;
    QStringList parts = data.split("-");
    int numPairs = parts.size() / 2;
    for (int i = 0; i < numPairs; ++i) {
        QString macAddress = parts[i * 2];
        QString rssi = "-" + parts[i * 2 + 1];
        wifiMac = wifiMac.toUpper();
        // qDebug() << getIndex() << "dongle的的wifiMac:" << macAddress;
        // qDebug() << getIndex() << "RSSI:" << rssi;
        // qDebug() << getIndex() << " 牙刷的wifiMac:" << wifiMac;
        if (macAddress == wifiMac) {
            ui->WIFI_RSSI->setText("WIFI的RSSI：" + rssi);
            // qDebug() << getIndex()<< getIndex() << " 比对成功";
            refreshWifiState(1);
            WIFI_RSSI = rssi;
            bool ok;
            WIFI_RSSI.toInt(&ok);

            if (!ok) {
                qDebug() << "转换WIFIrssi失败,内容为" + WIFI_RSSI + "内容结束";
            } else {
                //  showlog("转换成功");
                intwifirssi = WIFI_RSSI.toInt(&ok);
            }
        }
    }
}
void QFreeWork::initDate() {
    ui->tail_sn->setText("芯片存储的尾盖sn:");
    ui->bleStatusLabel->setText("蓝牙连接：");
    rssitestcount = 0;

    rssitestfailcount = 0;
    wifistate = 0;
    measure_ammeter = 0;
    dongleOutTime = 10;
    canGoNext = 1;
    isovertime = 0;
    BLE_RSSI = "";
    WIFI_RSSI = "";
    is_battary_test = 0;
    charageresult = "未测";
    voltageresult = "未测";
    currentresult = "未测";
    pb->reset_all_pb();
    at->resetConnected();
    TestResult = passValue;
    wifiresult = "未测";
    bleresult = "未测";
    ui->battary_state->setText("充电状态为:");
    ui->battary_value->setText("电量为:");
    ui->battary_voltage->setText("电压为:");
    stringsn = "";
    TestTime.start();
}

void QFreeWork::on_pushButton_clicked() {
    // ui->macInput->setText("f4:12:fa:c5:51:c6");
    // // ui->macInput->setText("74:4D:BD:95:7D:EA");//wd牙刷
    // // ui->macInput->setText("3c:84:27:06:f7:5e");
    ui->macInput->setText("3C:84:27:07:A8:D2");
    // // ui->macInput->setText("3c:84:27:29:50:32");
    ui->macInput->setText("b4:56:5d:bf:57:9d");

    on_macInput_returnPressed();
    // // usb-> getlxMEASure();
    // // waitWork(1000);

    // showlog("正在获取牙刷电量");
    // ui->comNameCombo->setCurrentText("COM134");
}

void QFreeWork::on_get_battery_clicked() {
    if (at->getConnected()) {
        pb->get_battery();
        showlog("正在获取牙刷电量");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void QFreeWork::on_disconnectwifi_clicked() {
    if (at->getConnected()) {
        pb->set_wifi_disconnect();
        showlog("已发送断开wifi");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}
void QFreeWork::on_connectwifi_clicked() {
    QString wifiName = SETTINGS.value(QString("WIFI/Name%1").arg(getIndex())).toString();
    QString wifiPassword = SETTINGS.value("WIFI/Password").toString();

    // QString wifiName = SETTINGS.value("WIFI/Name").toString();

    QByteArray wifiNameBytes = wifiName.toUtf8();
    QByteArray wifiPasswordBytes = wifiPassword.toUtf8();

    if (at->getConnected()) {
        pb->set_connect_wifi(wifiNameBytes, wifiPasswordBytes);
        showlog("已发送连接wifi");
    } else {
        showlog("请等待连接牙刷后再试");
    }
}

void QFreeWork::on_macInput_returnPressed() {
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }

    if (pack.factory == "lx" || pack.factory == "jj") {
        if (!usbSerialPort->isOpen()) {
            openUsbSerialPort();
        }
    }

    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = ui->macInput->text();
        ui->macLabel->setText("蓝牙mac: " + macAddress);

        ui->test_result->setText("WAIT");
        ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: "
                                       "10px; padding: 10px; text-align: center; ");

        ui->start_wifible_test->setEnabled(false);
        // 主状态机流程
        isTestContinue = true;
        teststate = -1;

        emit send_go_next_focus();
        ui->getMac->setDisabled(1);
        ui->macInput->setDisabled(1);
    }
}

void QFreeWork::on_pushButton_2_clicked() {
    at->sendBLELOG(1);  // 日志开
}

void QFreeWork::on_getMac_returnPressed() {
    testResultTableInit();

    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 40px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");

    // 检查是否是序列号格式
    QRegularExpression snRegex(snPattern);
    // 使用正则表达式匹配
    if (!snRegex.match(ui->getMac->text()).hasMatch()) {
        showlog("序列号错误");
        showlog("实际长度为" + QString::number(ui->getMac->text().length()));
        showlog("要求格式为" + snPattern);
        ui->getMac->clear();
        return;
    }
    sn = ui->getMac->text().toUtf8();
    showlog("正在查询mac地址");
    getMac(ui->getMac->text());             // 文件获取
    processInspection(ui->getMac->text());  // 站前检测
    processGetMesTestValue();               // mes获取
}

void QFreeWork::processInspection(QString stringsn) {
    if (stringsn != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            showlog("正在进行站前检测");
            pack.sn = stringsn;

            pack.mechines = getIndex();

            pack.is_hq_send_mac = 0;
            pack.instruct_num = "079";
            emit sendProcessInspection(pack);
        }
    } else {
        showlog("SN比对错误");
    }

    if (!ui->isusemes->checkState())  // 离线
    {
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet("font-size: 33px; background-color: #FFFF00; color: black; border: 2px solid "
                                     "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}

void QFreeWork::processGetMesTestValue() {
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();

        pack.is_hq_send_mac = 1;

        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit getMesTestValue(pack);
    }
}
void QFreeWork::getMac(QString sn_to_search) {
    QFile file("mac_sn.txt");              // 创建一个文件对象
    if (file.open(QIODevice::ReadOnly)) {  // 打开文件
        QTextStream in(&file);
        while (!in.atEnd()) {                      // 逐行读取文件
            QString line = in.readLine();          // 读取一行
            QStringList fields = line.split(",");  // 将行按照逗号分隔成两个字段
            if (fields.count() >= 2) {
                QString sn = fields.at(0);   // 第一个字段是sn
                QString mac = fields.at(1);  // 第二个字段是mac
                if (sn == sn_to_search) {    // 检查是否是待检索的sn
                    {
                        ui->macInput->setText(mac);
                        on_macInput_returnPressed();
                        showlog("这是从文件获取的mac地址");
                        qDebug() << getIndex() << "The corresponding mac is: " << mac;
                    }

                    break;
                }
            }
        }

        file.close();  // 关闭文件
    }
    if (!ui->isformmes->isChecked() && ui->macInput->text().isEmpty()) {
        ui->getMac->clear();
        showlog("找不到mac地址，清空当前输入的sn");
    }
}
void QFreeWork::getmacadress(const QByteArray& byte) {
    receivedData = "";
    receivedData = receivedData + QString::fromUtf8(byte);

    if (receivedData.length() >= 2 && receivedData.right(2) == "\r\n") {
        // 使用正则表达式提取设备信息
        QRegularExpression regex("deviceName:(.*?),\\s*deviceAddress:(.*?),\\s*deviceRssi(?:\\s*:(.*))?");
        QRegularExpressionMatch match = regex.match(receivedData);
        QString deviceName, deviceAddress, deviceRssi;

        // qDebug() << getIndex()<< "数据长度" << receivedData;
        // 检查是否匹配成功
        if (match.hasMatch()) {
            deviceName = match.captured(1).trimmed();
            deviceAddress = match.captured(2).trimmed();
            deviceRssi = match.captured(3).trimmed();
            // qDebug() << getIndex()<< "设备名称：" << deviceName;
            // qDebug() << getIndex()<< "设备地址：" << deviceAddress;
            // qDebug() << getIndex()<< "设备RSSI：" << deviceRssi;

            deviceMap[deviceAddress]["Name"] = deviceName;
            deviceMap[deviceAddress]["Rssi"] = deviceRssi;

            updateComboBox();
        }
        receivedData.clear();
    }
}
void QFreeWork::on_clear_scan_clicked() {
    deviceMap.clear();
    ui->mac_combo->clear();
}
void QFreeWork::updateComboBox() {
    // 遍历设备信息，根据 rssi 的值进行过滤
    for (auto it = deviceMap.begin(); it != deviceMap.end(); ++it) {
        QString deviceAddress = it.key();
        QString deviceName = it.value()["Name"];
        QString deviceRssi = it.value()["Rssi"];

        if (deviceName.contains(ui->name_range->text()) && deviceRssi.toInt() > ui->rssi_range->text().toInt() &&
            deviceAddress.length() == 17)

        {
            int index = ui->mac_combo->findText(deviceAddress);
            qDebug() << getIndex() << "信号强度:" << deviceRssi;
            if (index == -1) {
                ui->mac_combo->addItem(deviceAddress);
                qDebug() << getIndex() << "有新增" << deviceAddress;
            }
        }
    }
}
void QFreeWork::on_mac_combo_textActivated(const QString& arg1) {
    ui->log->clear();
    ui->msgEdit->clear();
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(arg1).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", "Mac地址错误");
        return;
    } else {
        macAddress = arg1;
        // at->sendMac(macAddress);//发送mac地址
        qDebug() << getIndex() << macAddress;
        bandingMacSn(macAddress, snbanding);
    }
    ui->snbanding->setFocus();
}
void QFreeWork::bandingMacSn(QString bandingmac, QString bandingsn) {
    if (bandingsn == "" || bandingmac == "")
        bandingresult = false;

    QString path = "\\\\10.196.200.51\\sgpub\\LTC\\Q20-OTA\\mac_sn.txt";
    QFileInfo checkPath(path);
    if (checkPath.exists() && checkPath.isDir()) {
        path = "\\\\10.196.200.51\\sgpub\\LTC\\Q20-OTA\\mac_sn.txt";
        qDebug() << "The network path exists and is a directory.";
    } else {
        path = "mac_sn.txt";
        qDebug() << "The network path does not exist or is not a directory.";
    }

    // 在 Windows 上，使用 QDir::fromNativeSeparators 将路径中的反斜杠转换为正斜杠
    // path = QDir::fromNativeSeparators(path);
    QFile file(path);  // 创建一个文件对象

    if (file.open(QIODevice::ReadWrite | QIODevice::Text)) {  //
        QTextStream in(&file);                                // 创建一个文本流对象
        QStringList lines;                                    // 用于存储文件中的每一行数据
        bool found = false;                                   // 标记是否找到了相同的SN
        while (!in.atEnd()) {
            QString line = in.readLine();         // 逐行读取文件
            QStringList parts = line.split(",");  // 以逗号分隔每行数据
            if (parts.size() == 2 && parts[0].trimmed() == bandingsn) {
                // 如果找到了相同的SN，替换MAC地址
                lines << (bandingsn + "," + bandingmac);
                found = true;
            } else {
                // 否则，保留原有数据
                lines << line;
            }
        }
        if (!found) {
            // 如果没有找到相同的SN，则追加新的SN和MAC地址
            lines << (bandingsn + "," + bandingmac);
        }
        // 清空文件并写入新的数据
        file.resize(0);
        QTextStream out(&file);
        for (const QString& line : lines) {
            out << line << endl;
        }
        file.close();  // 关闭文件
        showlog("保存mac_sn文件成功");
    } else {
        showlog("保存mac_sn文件失败");
    }

    // QFile file("mac_sn.txt");   // 创建一个文件对象
    // if (file.open(QIODevice::ReadWrite | QIODevice::Append))
    // {                                                    //
    //     QTextStream out(&file);                          // 创建一个文本流对象
    //     out << bandingsn << "," << bandingmac << endl;   // 将sn和mac写入文件
    //     file.close();                                    // 关闭文件
    // }

    bandingMacSn_mes(bandingmac, bandingsn);
}

void QFreeWork::bandingMacSn_mes(QString bandingmac, QString bandingsn) {
    pack.mechines = 1;  // 1脱1,1号上位机
    pack.sn = snbanding;
    pack.result = "PASS";
    pack.itemvalue = QString("|BTMAC:%1|").arg(bandingmac);
    pack.instruct_num = "076";
    if (ui->isusemes->checkState()) {
        send_end_testPass(pack);
    }

    if (bandingresult) {
        ui->banding_result->setText("绑定:PASS");
        ui->banding_result->setStyleSheet("font-size: 33px; background-color: #00FF00; color: black; border: 2px solid "
                                          "#00FF00; border-radius: 10px; padding: 10px; text-align: center;");
    } else {
        ui->banding_result->setText("绑定:NG");
        ui->banding_result->setStyleSheet("font-size: 33px; background-color: #FF0000; color: black; border: 2px solid "
                                          "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
    ui->snbanding->setFocus();
}
void QFreeWork::on_snbanding_returnPressed() {
    ui->banding_result->setText("绑定:WAIT");
    ui->banding_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");

    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    snbanding = ui->snbanding->text();
    at->sendMac("00:00:00:00:00:00");  // 发送mac地址
    ui->snbanding->clear();
    bandingresult = true;
}

void QFreeWork::getTestValue(const int mechines, const QString value) {
    // showlog(value);
    QString mesmacAddress;
    if (pack.factory == "hq") {
        // 定义正则表达式，匹配MAC地址的模式
        QRegularExpression regex("\"BTMAC\":\\s*\"([0-9A-Fa-f:]+)\"");

        // 在数据中查找匹配的内容
        QRegularExpressionMatch match = regex.match(value);

        // 检查是否有匹配项
        if (match.hasMatch()) {
            // 提取MAC地址
            mesmacAddress = match.captured(1);
            qDebug() << getIndex() << "MAC地址：" << mesmacAddress;
            if (mechines == getIndex()) {
                ui->macInput->setText(mesmacAddress);
                on_macInput_returnPressed();
            }
        } else {
            showlog("mes未找到匹配的MAC地址");
            showlog(value);
        }
    }
    // showlog(value);
    else if (pack.factory == "lx") {
        mesmacAddress = value;

        // 在2、4、6、8、10的位置插入冒号
        mesmacAddress.insert(2, ":");
        mesmacAddress.insert(5, ":");
        mesmacAddress.insert(8, ":");
        mesmacAddress.insert(11, ":");
        mesmacAddress.insert(14, ":");

        // 将小写字母转换成大写字母
        mesmacAddress = mesmacAddress.toUpper();
        if (mechines == getIndex()) {
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    } else {
        if (mechines == getIndex()) {
            mesmacAddress = value;
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    }

    // bandingMacSn(mesmacAddress, ui->getMac->text());//获取测试数据不要绑定测试mac——sn
}
void QFreeWork::on_clear_nfc_data_clicked() {
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8] = {0x03, 0x00, 0xFE, 0x00};  // 写入数据缓冲区
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    icdev = dc_init(100, 115200);
    if ((intptr_t)icdev <= 0) {
        showlog("Init Com Error!");
        return;
    } else {
        showlog("Init Com OK!");
    }
    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0) {
        showlog("dc_card_n Error!");
        return;
    } else {
        showlog("dc_card_n Ok!");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char*)szSnr;
        showlog(QString::fromStdString(str1));
    }

    int ret = dc_write(icdev, 4, writedata);  // 将写入数据缓冲区中的数据写入设备

    if (ret != 0) {
        QString errMsg = QString("数据写入错误，ret = %1").arg(ret);
        qDebug() << getIndex() << "errMsg: " << writedata << errMsg;
    }

    st = dc_read(icdev, 4, rdata);
    if (st != 0) {
        showlog("dc_read Error!");
        return;
    } else {
        memset(rdatahex, 0x00, sizeof(rdatahex));
        hex_a(rdata, rdatahex, 4);
        std::string str1 = (char*)rdatahex;
        showlog(QString::fromStdString(str1));
    }

    if ((intptr_t)icdev > 0) {
        st = dc_exit(icdev);
        if (st != 0) {
            showlog("dc_exit Error!");
            return;
        } else {
            showlog("dc_exit OK!");
            icdev = (HANDLE)-1;
        }
    }
    return;
}
QString QFreeWork::generateDateCode() {
    // 获取当前日期时间
    QDateTime currentDateTime = QDateTime::currentDateTime();

    // 获取当前年份、月份和日期
    int year = currentDateTime.date().year() % 100;  // 获取后两位年份
    int month = currentDateTime.date().month();
    int day = currentDateTime.date().day();

    // 月份编码
    char monthCode;
    if (month >= 1 && month <= 9) {
        monthCode = '0' + month;
    } else {
        monthCode = 'A' + (month - 10);
    }

    // 日期编码
    char dayCode;
    if (day >= 1 && day <= 9) {
        dayCode = '0' + day;
    } else {
        dayCode = 'A' + (day - 10);
    }

    // 构造日期编码
    QString dateCode = QString::number(year) + monthCode + dayCode;
    return dateCode;
}
QString QFreeWork::generateHexString(int productionNumber) {
    // 构造生产数量字符串（4位数，不足位数前面补0）
    // QString productionStr = QString("%1").arg(productionNumber, 4, 16, QChar('0'));

    // // 构造十六进制字符串
    // QString hexString = sku;           // 固定部分
    // hexString += generateDateCode();   // 日期部分
    // hexString += productionStr;        // 生产数量
    // qDebug() << getIndex() << "nfc的序列号: " << hexString;

    return "hexString";
}
void QFreeWork::on_nfc_write_read_clicked() {
    // TODO: 在此添加控件通知处理程序代码
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    unsigned char writedata[8];  // 写入数据缓冲区
    ReadNfcData = "";
    // 033BD2023668772001004800324F30450081080027200014178591141035353034303730313233313131313131170102910B0101010A063C842707A8D1
    // 3C842707A8D1
    // 35353034303730313233313131313131
    // 5504070123111111    //55040701华为固定开头sku2311年月日1111数量
    QString hexString;

    static int productionNumber = ui->nfc_count->text().toInt();  // 记录生产数量
    // QString text = generateHexString(productionNumber++);//自己生
    QString text = ui->nfc_sn->text();  // 外部给
    ui->nfc_count->setText(QString::number(productionNumber));

    for (int i = 0; i < text.length(); ++i) {
        hexString += QString("%1").arg(text[i].toLatin1(), 2, 16, QChar('0'));
    }

    QString dataText = "033BD2023668772001004800324F304500810800272000141785911410" + hexString +
                       "170102910B0101010A06" + macAddress.remove(":").toUpper();

    QByteArray dataBytes = QByteArray::fromHex(dataText.toLatin1());  // 将十六进制字符串转换为字节数组
    int dataSize = dataBytes.size();                                  // 获取字节数组的大小
    qDebug() << getIndex() << "dataSize: " << dataSize << dataText;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    icdev = dc_init(100, 115200);
    if ((intptr_t)icdev <= 0) {
        showlog("初始化nfc接口失败!");
        TestResult = failValue;
        return;
    } else {
        showlog("初始化nfc接口成功");
    }
    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0) {
        if (st == 1)
            showlog("nfc卡识别不到");
        if (st < 0)
            showlog("nfc卡查询失败");

        TestResult = failValue;
        return;
    } else {
        showlog("nfc卡查询成功");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char*)szSnr;
        showlog("卡的序列号为" + QString::fromStdString(str1));
    }

    for (int i = 0; i < dataSize; i += 4) {        // 每次处理8个字节
        int bytesToWrite = qMin(8, dataSize - i);  // 计算本次需要写入的字节数，最多8个

        memcpy(writedata, dataBytes.constData() + i, bytesToWrite);  // 将数据复制到写入数据缓冲区

        int ret = dc_write(icdev, 4 + i / 4, writedata);  // 将写入数据缓冲区中的数据写入设备

        if (ret != 0) {
            QString errMsg = QString("数据写入错误，ret = %1").arg(ret);
            showlog(errMsg);

            qDebug() << getIndex() << "errMsg: " << writedata << errMsg;
        }
    }
    showlog("nfc信息读取内容为：");
    for (int i = 4; i * 4 < dataSize; i += 4) {  // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0) {
            // showlog("dc_read Error!");
            showlog("nfc信息读取失败");
            TestResult = failValue;
            return;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            showlog(QString::fromStdString(str1));
        }
    }
    if (dataSize % 16) {
        st = dc_read(icdev, 4 + (dataSize / 16) * 4, rdata);
        if (st != 0) {
            showlog("nfc信息读取失败");
            TestResult = failValue;
            //  showlog("dc_read Error!");
            return;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, dataSize % 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            showlog(QString::fromStdString(str1));
        }
    }
    showlog("nfc信息读取结束");
}
void QFreeWork::on_nfc_sn_returnPressed() { ui->getMac->setFocus(); }

void QFreeWork::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void QFreeWork::on_disconnectButton_clicked() {
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    closeDongleSerialPort();
}

void QFreeWork::on_stopTest_clicked() {
    // at->sendMac("00:00:00:00:00:00");   // 发送mac地址
    // waitWork(100);
    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->getMac->setFocus();
    on_disconnectButton_clicked();
}

void QFreeWork::on_save_config_clicked() { showTestIndexes(); }
