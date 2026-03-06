
#include "factory_analyzer.h"
// 定义一个函数，用于执行具有特定名称的函数
void factory_analyzer::executeFunctionByName(const QString functionName) {
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
void factory_analyzer::createTestFunctions() {
    testFunctions = {
                     // {"禁止休眠", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_forbid_sleep, pb, FacSwitch_OPEN)); }},
                     // {"蓝牙升级",
                     //  [&]() {
                     //      // sendCommandWithRetry(std::bind(&Qpb::set_forbid_sleep, pb, FacSwitch_OPEN));
                     //  }},
                     // {"电机升级",
                     //  [&]() {
                     //      // sendCommandWithRetry(std::bind(&Qpb::set_forbid_sleep, pb, FacSwitch_OPEN));
                     //  }},
                     // {"压感升级",
                     //  [&]() {
                     //      // sendCommandWithRetry(std::bind(&Qpb::set_forbid_sleep, pb, FacSwitch_OPEN));
                     //  }},
                     // {"打开串口接收", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_uart_receive, pb, 1)); }},
                     // {"关闭串口接收", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_uart_receive, pb, 0)); }},
                     // {"设置屏幕颜色", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_screen_color, pb, 1)); }},
                     // {"获取尾盖SN码", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_sn, pb, FacDevInfoType_TAIL_SN)); }},
                     // {"获取基本信息", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_base_info, pb)); }},
                     // {"获取电量信息", [&]() { sendCommandWithRetry(std::bind(&Qpb::get_battery, pb)); }},
                     // {"进入船运模式", [&]() { sendCommandWithRetry(std::bind(&Qpb::set_ship_mode, pb, 1)); }},


{"检查sd卡有没有拔掉", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_sd_unset.sh",2000)); }},
{"读取整机sn", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "dji_sn_ops.sh device RD",2000)); }},
{"读取核心板sn", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "dji_sn_ops.sh board RD",2000)); }},
{"高通控制机器红灯", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_rgb_leds.sh red",2000)); }},
{"高通控制机器绿灯", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_rgb_leds.sh green",2000)); }},
{"测试rtc走时功能", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_rtc_good.sh",20000)); }},
{"读取ufs固件版本号", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_ufs_version.sh",2000)); }},
{"高通温度检查", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_read_ntc.sh",2000)); }},
{"电源按键检查", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_button_good.sh power_key",2000)); }},
{"读取硬件版本号", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_read_hw_ver.sh",2000)); }},
{"pmic寄存器检查", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_pmic_regs.sh",2000)); }},
{"rtc中断检查", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_wakealarm_interrupts.sh",8000)); }},
{"ufs跌落值写入", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_ufs_value.sh write 1",2000)); }},
{"ufs跌落值读取", [&]() { sendCommandWithRetry(std::bind(&QBulk::set_amt_task_test, bulk, "test_ufs_value.sh read 1",2000)); }},


                     };
}
int factory_analyzer::sendCommandWithRetry(std::function<void()> commandFunc) {
    static int retryCount = 0;
    canGoNext = false;
    sendRetryOver = false;
    if (commandFunc != nullptr) {
        // showlog("发送pb初始指令");
        commandFunc();  // 重新发送指令
    }
sendCommand_test_result=true;
    // 启动定时器
    QTimer* timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, [=]() {
        QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
        // qDebug() << "retryCount=" << retryCount
        //          << QString("sendCommandWithRetry定时器触发时间: %1, timer 地址: %2")
        //                 .arg(currentTime)
        //                 .arg(reinterpret_cast<quintptr>(timer), 0, 16);  // 以16进制显示地址

        if (!getRespone) {          // 根据传递进来的条件判断是否未收到响应
            if (retryCount < 20) {  // 如果还有重试次数
                if (commandFunc != nullptr && !(retryCount % 5)) {
                    showlog("重新发送指令测试" + QString::number(retryCount));
                    commandFunc();  // 重新发送指令
                }
                retryCount++;
            } else {
                disconnect(timer, &QTimer::timeout, this, nullptr);
                getRespone = 0;
                retryCount = 0;
                sendRetryOver = 1;
                timer->stop();  // 达到最大重试次数，停止定时器
                sendCommand_test_result=false;
                showlog("达到最大重试次数，停止定时器");
                delete timer;
                return 0;
            }
        } else {  // 如果收到响应
            disconnect(timer, &QTimer::timeout, this, nullptr);
            timer->stop();
            delete timer;
            retryCount = 0;
            getRespone = 0;
            canGoNext = 1;

            showlog("sendCommandWithRetry完成，收到设备响应");
            return 1;
        }
        return 0;
    });

    timer->start(300);  // 启动定时器
    return 0;
}
void factory_analyzer::testResultTableInit() {
    // pb->APP_VERSION = upperComputerVer;
    // LockProductUI();
    if (testResultTable() == nullptr) {
        showlog("testResultTableInit不存在表格");
        return;
    }
    testResultTable()->clear();
    // 初始化表格
    testResultTable()->setColumnCount(4);  // 三列，分别为Mac地址、SN码和时间戳

    testResultTable()->setRowCount(0);  // 初始行数为0，因为还没有数据

    // 设置表格标题
    QStringList headers;
    headers << "项目"
            << "数据"
            << "结果"
            << "要求";
    testResultTable()->setHorizontalHeaderLabels(headers);
    // 设置表格自适应列宽
    testResultTable()->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
}
void factory_analyzer::reorderCheckBoxes() {
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
QVector<int> factory_analyzer::loadIndexesFromConfig() {
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
DraggableCheckBox* factory_analyzer::getCanUseCheckBoxByIndex(int index) {
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
DraggableCheckBox* factory_analyzer::getCheckBoxByIndex(int index) {
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

int factory_analyzer::getIndexAt(const QPoint& pos) {
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
void factory_analyzer::showTestIndexes() {
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
void factory_analyzer::saveIndexesToConfig(const QVector<int>& indexes) {
    SETTINGS.beginGroup("TestOrder");  // 创建一个分组
    SETTINGS.remove("");               // 清空当前组的所有项，以免覆盖
    for (int i = 0; i < indexes.size(); ++i) {
        SETTINGS.setValue(QString::number(i), indexes[i]);  // 将每个索引存入配置文件
    }
    SETTINGS.endGroup();
}

// 获取下一个状态的函数
factory_analyzer::State factory_analyzer::getNextState(State currentState) {
    return static_cast<State>((static_cast<int>(currentState) + 1) % 5);
}
void factory_analyzer::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else if (event->mimeData()->hasText()) {  // 如果拖动的数据有文本
        // qDebug() << "拖动数据有文本Enter";
        event->acceptProposedAction();  // 接受拖动操作
        // qDebug() << "拖动数据有文本Enter";
    }else{
           event->ignore();
    }

}


void factory_analyzer::moveToLayout(QLayout* fromLayout, QLayout* toLayout, QWidget* widget) {
    if (fromLayout)
        fromLayout->removeWidget(widget);
    toLayout->addWidget(widget);
}

void factory_analyzer::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);

    // 只创建一个 QPainter 对象
    QPainter painter(this);
    // 获取 can_use 控件的几何信息
    QRect canUseLayoutArea = ui->can_use->geometry();

    // singleCheckBoxHeight = (canUseLayoutArea.height() - (2 * MARGIN) + ROW_SPACING) /
    //                        canUserRow;  // checkBoxes[0]->height() + ROW_SPACING;

    singleCheckBoxWidth =
        (canUseLayoutArea.width() - 2 * MARGIN + COLUMN_SPACING) / canUserCol;  // checkBoxes[0]->width() +
    // COLUMN_SPACING; 绘制矩形在拖动位置
    if (!dragPos.isNull()) {
        painter.fillRect(
            QRect(dragPos, QSize(singleCheckBoxWidth - COLUMN_SPACING, singleCheckBoxHeight - ROW_SPACING)), Qt::green);
    }
}

void factory_analyzer::dropEvent(QDropEvent* event) {
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
    qDebug() << "放下了" << event->pos();
    QPoint mouseGlobalPos = mapToGlobal(event->pos());
    qDebug() << "鼠标全局位置:" << mouseGlobalPos;

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
        if (row < 0 || col < 0) {
            qWarning() << "非法 row/col，取消拖动";
            return;
        }
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
void factory_analyzer::dragMoveEvent(QDragMoveEvent* event) {
    if (event->mimeData()->hasText()) {  // 如果拖动的数据有文本
        dragPos = event->pos();//这个是相对于我qt的坐标
        update();
        event->acceptProposedAction();  // 接受拖动操作
        // qDebug() << "界面全局坐标"<<dragPos;
    }
}


void factory_analyzer::calculateGridPosition(const QPoint& globalPos,
                                             const QRect& area,
                                             int& row,
                                             int& col)

{
    row = col = -1;

    // 1️⃣ 宿主 widget（和 layout 同一个坐标系）
    QWidget* host = ui->can_use;

    // 2️⃣ 全局 → host 坐标
    QPoint pos = host->mapFromGlobal(globalPos);

    // 3️⃣ layout 几何信息
    QGridLayout* layout = canUselayout;

    QRect layoutRect = layout->geometry();

    int rows = layout->rowCount();
    int cols = layout->columnCount();

    if (rows <= 0 || cols <= 0)
        return;

    // 4️⃣ 计算单元格尺寸
    int hSpacing = layout->horizontalSpacing();
    int vSpacing = layout->verticalSpacing();

    int cellW = (layoutRect.width()  + hSpacing) / cols;
    int cellH = (layoutRect.height() + vSpacing) / rows;

    // 5️⃣ 转成 layout 内坐标
    QPoint inLayout = pos - layoutRect.topLeft();

    if (!QRect(QPoint(0, 0), layoutRect.size()).contains(inLayout))
        return;

    // 6️⃣ 计算 row / col
    col = inLayout.x() / cellW;
    row = inLayout.y() / cellH;

    // 7️⃣ 防越界
    row = qBound(0, row, rows - 1);
    col = qBound(0, col, cols - 1);

    qDebug().noquote()
        << "[GridCalc OK]"
        << "global=" << globalPos
        << "mapped=" << pos
        << "layoutRect=" << layoutRect
        << "cell=" << cellW << "x" << cellH
        << "row/col=" << row << col;
}


// void factory_analyzer::calculateGridPosition(const QPoint& globalPos,
//                                              const QRect& area,
//                                              int& row,
//                                              int& col)
// {

//    // 393-1634
//     int singleCheckBoxHeight =
//         (area.height() - 2 * MARGIN + ROW_SPACING) / canUserRow;
//     int singleCheckBoxWidth  =
//         (area.width()  - 2 * MARGIN + COLUMN_SPACING) / canUserCol;

//     int relativeY = globalPos.y() - area.y() - MARGIN;
//     int relativeX = globalPos.x() - area.x() - MARGIN;

//     int rawRow = relativeY / singleCheckBoxHeight;
//     int rawCol = relativeX / singleCheckBoxWidth;

//     row = qBound(0, rawRow, canUselayout->rowCount() - 1);
//     col = qBound(0, rawCol, canUselayout->columnCount() - 1);

//     qDebug().noquote()
//         << "[GridCalc]"
//         << "\n  globalPos =" << globalPos
//         << "\n  area      =" << area
//         << "\n  margin    =" << MARGIN
//         << "\n  row/col   =" << canUserRow << "/" << canUserCol
//         << "\n  cell(w,h) =" << singleCheckBoxWidth << "," << singleCheckBoxHeight
//         << "\n  relative  =" << relativeX << "," << relativeY
//         << "\n  rawRow/rawCol =" << rawRow << "," << rawCol
//         << "\n  final row/col =" << row << "," << col;
// }



void factory_analyzer::moveToGrid(QGridLayout* layout, QWidget* widget, int row, int col) {
    if (!layout)
        return;
    if (layout->indexOf(widget) != -1)
        layout->removeWidget(widget);
    layout->addWidget(widget, row, col);
}

