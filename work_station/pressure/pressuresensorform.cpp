#include "pressuresensorform.h"

#include <QHBoxLayout>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QVBoxLayout>

#include "AbIni.h"
#include "fixture_uart.h"
#include "ui_pressuresensorform.h"

extern "C"  // 由于是C版的dll文件，在C++中引入其头文件要加extern "C" {},注意
{
#include "lib/nfc/dcrf32.h"
}

#if _MSC_VER >= 1600
#    pragma execution_character_set("utf-8")
#endif

// y20p
#define ADC_THRESHOLD_Y20P_BTH 350  // 200g:150 400g:350
#define ADC_THRESHOLD_Y20P_KEY 40

#define COUNT_THRESHOLD_Y20P_BTH 300
#define COUNT_THRESHOLD_Y20P_KEY 400

// f20
#define ADC_THRESHOLD_F20_BTH 60
#define ADC_THRESHOLD_F20_KEY 10

#define COUNT_THRESHOLD_F20_BTH 300
#define COUNT_THRESHOLD_F20_KEY 200

// u7
#define ADC_THRESHOLD_U7_KEY 40
#define COUNT_THRESHOLD_U7_KEY 200

// y21
#define ADC_THRESHOLD_Y21_BTH 70
#define COUNT_THRESHOLD_Y21_BTH 300

// p30p
#define ADC_THRESHOLD_P30P_KEY 40
#define COUNT_THRESHOLD_P30P_KEY 200

PressureSensorForm::PressureSensorForm(int index, QWidget* parent) : ui(new Ui::PressureSensorForm) {
    m_index = index;
    pack.mechines = getIndex();
    //  dongleBaudRate = 115200;
    dongleOutTime = 1;  // 太快会死锁
    upperComputerVer = PRESSURE_VER;

    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
    ui->uartStatusLabel->setText("串口连接：<font color='red'>失败</font>");
    ui->macLabel->setText("蓝牙mac:                        ");
    ui->frame_rate->setText("数据帧率:            ");

    for (auto i = 0; i < ui->horizontalLayout_14->count(); i++) {
        QWidget* w = ui->horizontalLayout_14->itemAt(i)->widget();
        if (w != nullptr) {
            w->setVisible(false);
        }
    }

    ui->botton_adc_diff->hide();

    QTextCharFormat fmt;
    // 字体色
    // fmt.setForeground(QBrush(QColor(255, 255, 255)));
    // fmt.setUnderlineColor("red");

    // 背景色
    fmt.setBackground(QBrush(QColor(255, 255, 255)));
    // 字体大小
    fmt.setFontPointSize(20);
    // 文本框使用以上设定
    ui->log->mergeCurrentCharFormat(fmt);

    on_refreshButton_clicked();
    ui->msgEdit->setStyleSheet("font-size: 16pt;");
    QSettings settings(SETTING_NAME, QSettings::IniFormat);

    mUserno = settings.value("Mes/mUserno").toString();
    machineNo = settings.value("Mes/machineNo").toString();
    ui->msgEdit->appendPlainText("mUserno=" + mUserno);
    ui->msgEdit->appendPlainText("machineNo=" + machineNo);

    actual_wait_time = settings.value("Time/TestTime").toInt();
    measure_wait_time = actual_wait_time + 5000;

    Product_Name = settings.value("ProductInfo/Product_Name").toString();
    product_model_init(Product_Name);

    Is_use_graph = settings.value("Graph/Use_graph").toString();
    graph_init(product_model, Is_use_graph);

    only_mode = settings.value("OnlyModule/Module", "ALL").toString();

    ui->mechine_number->setText(QString::number(getIndex() + 1) + "号机");
    ui->mechine_number->setStyleSheet("font-size: 33px; background-color: yellow; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 50px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 50px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    connect(pb, SIGNAL(send_press_data(FacUploadPresSensor)), this, SLOT(getPressSensorData(FacUploadPresSensor)));
    // connect(pb, SIGNAL(send_button_state(FacButtonState)), this, SLOT(getButtonState(FacButtonState)));
    // connect(pb, SIGNAL(send_sn_data(FacDevInfo)), this, SLOT(refresh_sn(FacDevInfo)));
    // connect(pb, SIGNAL(sendpresscalidata(FacPreSensorCalibResult)), this,
    // SLOT(getPresscalidata(FacPreSensorCalibResult))); connect(pb, SIGNAL(baseInfoReady(FacGetDevBaseInfo)), this,
    // SLOT(refreshBaseData(FacGetDevBaseInfo))); connect(at, SIGNAL(send_debug(QString)), this,
    // SLOT(LogPrintDebug(QString)));
    nfcComName = settings.value(QString("%1/nfcComName").arg(getIndex())).toString();

    IsSaveFail = settings.value("DATA/IsSaveFail").toInt();

    ui->FixturecomNameCombo->clear();
    foreach (const QSerialPortInfo& info, QSerialPortInfo::availablePorts()) {
        ui->FixturecomNameCombo->addItem(info.portName());
    }

    // connect(FixtureserialPort, SIGNAL(error(QSerialPort::SerialPortError)), this,
    // SLOT(FixturehandleError(QSerialPort::SerialPortError)));
    connect(waittime, &QTimer::timeout, [=]() {
        isovertime = 1;
        waittime->stop();
        qDebug() << "定时器结束计时" << QDateTime::currentDateTime();
    });
}

void PressureSensorForm::LogPrintDebug(QString debug) { ui->log->appendPlainText("LogPrint" + debug); }

void PressureSensorForm::graph_init(MODEL_ID_E model, QString is_use_graph) {
    if (is_use_graph == "USE_ALL_GRAPH") {
        graph_set_argu = GRAPH_SET_USE_ALL;
    } else if (is_use_graph == "USE_BTH_GRAPH") {
        graph_set_argu = GRAPH_SET_USE_BTH;
    } else if (is_use_graph == "USE_KEY_GRAPH") {
        graph_set_argu = GRAPH_SET_USE_KEY;
    } else {
        graph_set_argu = GRAPH_SET_CLOSE;
    }

    if (graph_set_argu <= GRAPH_SET_CLOSE) {
        delete (ui->frame);
        return;
    }

    qDebug() << "进入图标曲线初始化";
    int chnal_num = 0;
    if (model == MODEL_ID_Y20P) {
        chnal_num = 2;
    } else if (model == MODEL_ID_F20) {
        chnal_num = 2;
    } else if (model == MODEL_ID_U7) {
        chnal_num = 1;
    } else if (model == MODEL_ID_Y21) {
        chnal_num = 1;
    } else if (model == MODEL_ID_P30P) {
        chnal_num = 1;
    }

    for (int i = 0; i < chnal_num; i++) {
        QCustomPlot* plot_adc = new QCustomPlot;
        QCustomPlot* plot_value = new QCustomPlot;

        plot_adc->legend->setVisible(true);                            // 设置图例可见
        plot_adc->xAxis->setRange(0, 1000);                            // 设置 X 轴范围为 0 到 1000
        plot_adc->yAxis->setRange(0, 1000);                            // 设置 Y 轴范围为 0 到 1000
        plot_adc->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);  // 启用范围拖动和缩放
        plot_adc->addGraph();                                          // 添加图层
        plot_adc->graph(0)->setPen(QPen(Qt::red));                     // 设置图层画笔为红色
        plot_adc->graph(0)->setBrush(Qt::NoBrush);                     // 不填充颜色
        plot_adc->graph(0)->setAntialiased(true);                      // 不抗锯齿

        plot_adc->graph(0)->setName("ADC值");  // 设置图层名称
        plot_adc->xAxis->setLabel("时间（ms）");
        plot_adc->yAxis->setLabel("adc值");
        graph_adc_vector.push_back(plot_adc);

        // 创建压力值曲线图
        plot_value->legend->setVisible(true);                            // 设置图例可见
        plot_value->xAxis->setRange(0, 1000);                            // 设置 X 轴范围为 0 到 1000
        plot_value->yAxis->setRange(0, 1000);                            // 设置 Y 轴范围为 0 到 1000
        plot_value->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom);  // 启用范围拖动和缩放
        plot_value->addGraph();                                          // 添加图层
        plot_value->graph(0)->setPen(QPen(Qt::blue));                    // 设置图层画笔为蓝色
        plot_value->graph(0)->setBrush(QBrush(Qt::NoBrush));             // 使用蓝色填充
        plot_value->graph(0)->setAntialiased(true);                      // 不抗锯齿
        plot_value->graph(0)->setName("压力值");                         // 设置图层名称

        plot_value->xAxis->setLabel("时间（ms）");
        plot_value->yAxis->setLabel("压力值（g）");
        graph_value_vector.push_back(plot_value);
    }
    QVBoxLayout* vlayout = new QVBoxLayout(ui->frame);
    for (uint32_t n = 0; n < graph_adc_vector.size(); n++) {
        vlayout->addWidget(graph_adc_vector[n]);
    }
    for (uint32_t n = 0; n < graph_value_vector.size(); n++) {
        vlayout->addWidget(graph_value_vector[n]);
    }
}

void PressureSensorForm::calib_vector_init(MODEL_ID_E model) {
    //先清空容器里的元素
    if (!sensor_v.empty()) {
        // 清空 sensor_v
        sensor_v.clear();
        qDebug() << "sensor_v has been cleared.";
    } else {
        qDebug() << "sensor_v is already empty.";
    }
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    qDebug() << "初始化压感模组容器";

    if (model == MODEL_ID_Y20P) {
        ndt_sensor_cali y20p_ch0_vector;
        y20p_ch0_vector.ui_msg_tip[0] = donotmove;
        y20p_ch0_vector.ui_msg_tip[1] = brush_placing_200_weights;
        y20p_ch0_vector.ui_msg_tip[2] = pack_weights;
        y20p_ch0_vector.ui_msg_err[0] = "刷头错误码为：";
        y20p_ch0_vector.ui_msg_test[0] = "人员：请放50g砝码";
        y20p_ch0_vector.ui_msg_test[1] = "人员：请放350g砝码";
        y20p_ch0_vector.ui_msg_test[2] = "人员：请放450g砝码";

        ndt_sensor_cali y20p_ch1_vector;
        y20p_ch1_vector.ui_msg_tip[0] = donotmove;
        y20p_ch1_vector.ui_msg_tip[1] = botton_placing_200_weights;
        y20p_ch1_vector.ui_msg_tip[2] = "拿走按键200g砝码";
        y20p_ch1_vector.ui_msg_err[0] = "按键错误码为：";
        y20p_ch1_vector.ui_msg_test[0] = "人员：按键请放230g砝码";

        y20p_ch1_vector.sensor_test_status_set(TEST_NOTEST);

        // test_data_init
        // y20p_ch0_vector.test_time = 2;
        y20p_ch0_vector.para.test_grams[0] = 200;
        y20p_ch0_vector.para.test_grams[1] = 400;

        y20p_ch0_vector.para.test_tolerance[0] = 50;
        y20p_ch0_vector.para.test_tolerance[1] = 50;
        y20p_ch0_vector.para.test_count[0] = 300;
        y20p_ch0_vector.para.test_count[1] = 300;

        y20p_ch0_vector.para.adc_threshold = ADC_THRESHOLD_Y20P_BTH;
        y20p_ch1_vector.para.adc_threshold = ADC_THRESHOLD_Y20P_KEY;

        y20p_ch0_vector.para.count_threshold = COUNT_THRESHOLD_Y20P_BTH;
        y20p_ch1_vector.para.count_threshold = COUNT_THRESHOLD_Y20P_KEY;

        y20p_ch0_vector.para.f_module = MODULE_BTH;
        y20p_ch1_vector.para.f_module = MODULE_MODE_BUTTON;

        // y20p_ch1_vector.test_time = 1;
        y20p_ch1_vector.para.test_grams[0] = 200;
        y20p_ch1_vector.para.test_tolerance[0] = 50;
        y20p_ch1_vector.para.test_count[0] = 300;

        y20p_ch0_vector.para.lower_limit = settings.value("Limit/y20p_bth_lower", "1000").toInt();
        y20p_ch0_vector.para.upper_limit = settings.value("Limit/y20p_bth_upper", "3000").toInt();

        y20p_ch1_vector.para.lower_limit = settings.value("Limit/y20p_key_lower", "1000").toInt();
        y20p_ch1_vector.para.upper_limit = settings.value("Limit/y20p_key_upper", "3000").toInt();

        sensor_v.push_back(y20p_ch0_vector);
        sensor_v.push_back(y20p_ch1_vector);

        sensor_v[0].Sensor_cali_Init(CAL_CHANNEL_Y20_CH0);
        sensor_v[1].Sensor_cali_Init(CAL_CHANNEL_Y20_CH1);

    } else if (model == MODEL_ID_U7) {
        ndt_sensor_cali u7_ch0_vector;
        u7_ch0_vector.ui_msg_tip[0] = donotmove;
        u7_ch0_vector.ui_msg_tip[1] = "按键放砝码";
        u7_ch0_vector.ui_msg_tip[2] = "remove weight";

        u7_ch0_vector.ui_msg_test[0] = "人员：请放50g砝码";
        u7_ch0_vector.ui_msg_test[1] = "人员：请放350g砝码";
        u7_ch0_vector.ui_msg_test[2] = "人员：请放450g砝码";

        u7_ch0_vector.ui_msg_err[0] = "按键错误码为：";

        u7_ch0_vector.para.adc_threshold = ADC_THRESHOLD_U7_KEY;
        u7_ch0_vector.para.count_threshold = COUNT_THRESHOLD_U7_KEY;

        u7_ch0_vector.para.f_module = MODULE_MODE_BUTTON;

        u7_ch0_vector.para.lower_limit = settings.value("Limit/lower", "2500").toInt();
        u7_ch0_vector.para.upper_limit = settings.value("Limit/upper", "4500").toInt();

        sensor_v.push_back(u7_ch0_vector);

        sensor_v[0].Sensor_cali_Init(CAL_CHANNEL_U7_CH0);

    } else if (model == MODEL_ID_P30P) {
        ndt_sensor_cali p30p_ch0_vector;
        p30p_ch0_vector.ui_msg_tip[0] = donotmove;
        p30p_ch0_vector.ui_msg_tip[1] = "按键放砝码";
        p30p_ch0_vector.ui_msg_tip[2] = "remove weight";

        p30p_ch0_vector.ui_msg_err[0] = "刷头错误码为：";

        p30p_ch0_vector.ui_msg_test[0] = "测试100g不触发";
        p30p_ch0_vector.ui_msg_test[1] = "测试300g触发";

        p30p_ch0_vector.para.adc_threshold = ADC_THRESHOLD_P30P_KEY;
        p30p_ch0_vector.para.count_threshold = COUNT_THRESHOLD_P30P_KEY;

        p30p_ch0_vector.para.f_module = MODULE_POWER_BUTTON;

        p30p_ch0_vector.para.lower_limit = settings.value("Limit/power_button_lower", "500").toInt();
        p30p_ch0_vector.para.upper_limit = settings.value("Limit/power_button_upper", "4500").toInt();

        sensor_v.push_back(p30p_ch0_vector);

        sensor_v[0].Sensor_cali_Init(CAL_CHANNEL_P30P_CH0);
    }

    total_sensor = static_cast<int>(sensor_v.size());
    qDebug() << "model:" << model << "压感模组通道总数为：" << total_sensor;
}

void PressureSensorForm::system_cmd(QString command, system_command_e numb) {
    QProcess process;
    QString cmd = "test.bat";
    QStringList args;

    // args<< command;
    args.append(system_command_list[numb]);
    process.start(cmd, args);  // 启动可执行程序

    // 读取返回内容

    process.waitForStarted();
    process.waitForFinished();
    process.waitForReadyRead();
    QByteArray qba = process.readAll();
    QTextCodec* pTextCodec = QTextCodec::codecForName("System");
    assert(pTextCodec != nullptr);
    QString str = pTextCodec->toUnicode(qba);
    ui->log->appendPlainText(str);
}

void PressureSensorForm::getTestvalue(const int mechines, const QString value) {
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

void PressureSensorForm::getPresscalidata(FacPreSensorCalibResult x) {
    // qDebug() << "获取牙刷校准的brush_head_adc="<< x.brush_head_adc;
    // qDebug() << "获取牙刷校准的mode_button_adc="<< x.mode_button_adc;
    if (x.brush_head_adc != 0) {
        ui->log->appendPlainText("刷头" + QString::number(x.brush_head_adc));
        LastCali.calib_factor[MODULE_BTH] = x.brush_head_adc;
        qDebug() << "获取到刷头校准系数：" << LastCali.calib_factor[MODULE_BTH];
    }
    if (x.mode_button_adc != 0) {
        ui->log->appendPlainText("模式" + QString::number(x.mode_button_adc));
        LastCali.calib_factor[MODULE_MODE_BUTTON] = x.mode_button_adc;
        qDebug() << "获取到模式按键校准系数：" << LastCali.calib_factor[MODULE_MODE_BUTTON];
    }
    if (x.power_button_adc != 0) {
        ui->log->appendPlainText("电源" + QString::number(x.power_button_adc));
        LastCali.calib_factor[MODULE_POWER_BUTTON] = x.power_button_adc;
        qDebug() << "获取到电源按键校准系数：" << LastCali.calib_factor[MODULE_MODE_BUTTON];
    }
    if (x.temperature != 0) {
        ui->log->appendPlainText("温度" + QString::number(x.temperature));
        LastCali.temperature[MODULE_BTH] = x.temperature;
        qDebug() << "获取到温度系数：" << LastCali.calib_factor[MODULE_MODE_BUTTON];
    }

    if (x.brush_head_adc == (uint32_t)cali_result.calib_factor[MODULE_BTH] &&
        x.mode_button_adc == (uint32_t)cali_result.calib_factor[MODULE_MODE_BUTTON] &&
        x.power_button_adc == (uint32_t)cali_result.calib_factor[MODULE_POWER_BUTTON] &&
        (x.temperature == (uint32_t)cali_result.temperature[MODULE_BTH] ||
         x.temperature == (uint32_t)cali_result.temperature[MODULE_MODE_BUTTON])) {
        qDebug() << "校准数据核对成功";
        is_calib_suc = 1;
    }
}

PressureSensorForm::~PressureSensorForm() { delete ui; }

void PressureSensorForm::closeFixtureSerialPort() {
    // 启用RTS信号
    FixtureserialPort->setRequestToSend(false);
    // 启用DTR信号
    FixtureserialPort->setDataTerminalReady(false);
    if (FixtureserialPort->isOpen())
        FixtureserialPort->close();
    disconnect(FixtureserialPort, SIGNAL(readyRead()), this, SLOT(FixturereadData()));
    ui->FixturerefreshCom->setEnabled(true);
    ui->FixturecomNameCombo->setEnabled(true);
    ui->FixtureconnectButton->setEnabled(true);

    refresh_Fixtureuart_state(0);
}

void PressureSensorForm::refresh_Fixtureuart_state(int state) {
    if (state)
        ui->Fixtureuartstate->setText("治具串口连接：<font color='green'>成功</font>");
    else
        ui->Fixtureuartstate->setText("治具串口连接：<font color='red'>失败</font>");
}

void PressureSensorForm::on_FixtureconnectButton_clicked() {
    qDebug() << "FixtureconnectButton_clicked";
    openFixtureSerialPort();
}

void PressureSensorForm::on_FixturedisconnectButton_clicked() {
    qDebug() << "FixturedisconnectButton_clicked";
    closeFixtureSerialPort();
    ui->FixturerefreshCom->setEnabled(true);
    ui->FixtureconnectButton->setEnabled(true);
}

void PressureSensorForm::on_FixturerefreshCom_clicked() {
    qDebug() << "FixturerefreshCom_clicked";
    ui->FixturecomNameCombo->clear();
    foreach (const QSerialPortInfo& info, QSerialPortInfo::availablePorts()) {
        ui->FixturecomNameCombo->addItem(info.portName());
    }
}

void PressureSensorForm::openFixtureSerialPort(void) {
    qDebug() << "openFixtureSerialPort";
    if (FixtureserialPort->isOpen()) {
        disconnect(FixtureserialPort, SIGNAL(readyRead()), this, SLOT(FixturereadData()));
        FixtureserialPort->close();
    }

    // 设置串口名
    FixtureserialPort->setPortName(ui->FixturecomNameCombo->currentText());
    // 设置波特率
    FixtureserialPort->setBaudRate(115200);
    // 设置数据位
    FixtureserialPort->setDataBits(QSerialPort::Data8);
    // 设置校验位
    FixtureserialPort->setParity(QSerialPort::NoParity);
    // 设置停止位
    FixtureserialPort->setStopBits(QSerialPort::OneStop);
    FixtureserialPort->setReadBufferSize(4096);
    // 设置流控制
    FixtureserialPort->setFlowControl(QSerialPort::NoFlowControl);  // 设置为无流控制
    if (FixtureserialPort->open(QIODevice::ReadWrite)) {
        // 启用RTS信号
        FixtureserialPort->setRequestToSend(true);
        // 启用DTR信号
        FixtureserialPort->setDataTerminalReady(true);
        // ui->msgEdit->appendPlainText("Fixture串口连接成功");
        refresh_Fixtureuart_state(1);
        ui->FixturerefreshCom->setEnabled(false);
        ui->FixturecomNameCombo->setEnabled(false);
        ui->FixtureconnectButton->setEnabled(false);
        FixtureserialPort->setDataTerminalReady(true);
        connect(FixtureserialPort, SIGNAL(readyRead()), this, SLOT(FixturereadData()));
    } else {
        QMessageBox::warning(NULL, "警告", " 串口被占用！\t\r\n");
        //  ui->msgEdit->appendPlainText("打开错误");
    }
}

void PressureSensorForm::set_independent_state(STATE_INDEPENDENT_E state) {
    independent_state = state;
    qDebug() << "set_independent_state:" << independent_state;
}

STATE_INDEPENDENT_E PressureSensorForm::get_independent_state(void) { return independent_state; }

void PressureSensorForm::receive_uart_data(int state) {
    qDebug() << "receive_uart_data:" << state;
    if (state == 1) {
        getaskbarcode = 1;
    } else if (state == 2) {
        is_get_ready_command = true;
        set_independent_state(STATE_CALIB_START);
    } else if (state == 3) {
        if (getIndex() == 0) {
            set_independent_state(STATE_CALIB_START);
        }
    } else if (state == 4) {
        if (getIndex() == 1) {
            set_independent_state(STATE_CALIB_START);
        }
    }
}

void PressureSensorForm::FixturereadData() {
    QByteArray dataTemp;
    dataTemp = FixtureserialPort->readAll();
    // qusb->parseCmd(dataTemp);
    qDebug() << QString::fromUtf8(dataTemp);
    // 处理接收到的数据
    processReceivedData(dataTemp);
}

void PressureSensorForm::processReceivedData(const QByteArray& data) {
    if (data == "BarCode?") {
        getaskbarcode = 1;
        qDebug() << data;
    }

    if (data == "Ready") {
        is_get_ready_command = true;
        qDebug() << data;
    }

    if (data == "Set_OK") {
        is_get_set_ok_command = true;
        qDebug() << data;
    }

    if (data == "Pass")
        qDebug() << data;
    if (data == "Result OK")
        qDebug() << data;

    //  emit send_data_to_mechine(datapack);
}

void PressureSensorForm::send_mac(QString data) { ui->macLabel->setText("蓝牙mac: " + data); }

void PressureSensorForm::send_frame_rate(QString data) { ui->frame_rate->setText("数据帧率: " + data); }

void PressureSensorForm::refresh_uart_state(int state) {
    if (state)
        ui->uartStatusLabel->setText("串口连接：<font color='green'>成功</font>");
    else
        ui->uartStatusLabel->setText("串口连接：<font color='red'>失败</font>");
}

void PressureSensorForm::refresh_ble_state(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
    } else {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
    }
}

void PressureSensorForm::send_start_command(machine_command_id_e command_id, int argument) {
    FixturePacketData packet_data;

    packet_data.machine_command_id = command_id;
    packet_data.argument = argument;
    packet_data.machine_index = getIndex();

    qDebug() << "command_id:" << argument << "argument:" << argument;

    if (command_id == COMMAND_ID_BASE && argument == 1) {
        packet_data.machine_get_mac_state = 1;
    }

    if (command_id == COMMAND_ID_RESULT) {
        packet_data.machine_result_state = 1;
        packet_data.argument = argument;
    }
    // if (command_id == COMMAND_ID_KEY_PRESS_200)
    // {
    //     delay_msec(getIndex() * 200);
    // }
    emit send_data_to_mechine(packet_data);
}

void PressureSensorForm::FixturehandleError(QSerialPort::SerialPortError error) {
    qDebug() << "串口问题" << error;
    if (error == QSerialPort::PermissionError) {
        if (FixtureserialPort->isOpen())
            FixtureserialPort->close();
        disconnect(FixtureserialPort, SIGNAL(readyRead()), this, SLOT(FixturereadData()));
        ui->FixturerefreshCom->setEnabled(true);
        ui->FixturecomNameCombo->setEnabled(true);
        ui->FixtureconnectButton->setEnabled(true);
        QMessageBox::warning(NULL, "警告", " 治具串口被拔出！\t\r\n");
        ui->Fixtureuartstate->setText("串口连接：<font color='red'>拔出</font>");
    }
}

void PressureSensorForm::openSerialPort(void) {
    if (dongleSerialPort->isOpen()) {
        disconnect(dongleSerialPort, SIGNAL(readyRead()), this, SLOT(readData()));
        dongleSerialPort->close();
    }
    isTestContinue = true;
    // 设置串口名
    dongleSerialPort->setPortName(ui->comNameCombo->currentText());
    // 设置波特率
    dongleSerialPort->setBaudRate(921600);
    // 设置数据位
    dongleSerialPort->setDataBits(QSerialPort::Data8);
    // 设置校验位
    dongleSerialPort->setParity(QSerialPort::NoParity);
    // 设置停止位
    dongleSerialPort->setStopBits(QSerialPort::OneStop);
    dongleSerialPort->setReadBufferSize(4096);

    // 设置流控制
    dongleSerialPort->setFlowControl(QSerialPort::NoFlowControl);  // 设置为无流控制

    if (dongleSerialPort->open(QIODevice::ReadWrite)) {
        // 启用RTS信号
        dongleSerialPort->setRequestToSend(true);
        // 启用DTR信号
        dongleSerialPort->setDataTerminalReady(true);

        ui->msgEdit->appendPlainText("串口连接成功");
        refresh_uart_state(1);
        ui->refreshButton->setEnabled(false);
        ui->comNameCombo->setEnabled(false);
        ui->connectButton->setEnabled(false);
        dongleSerialPort->setDataTerminalReady(true);
        connect(dongleSerialPort, SIGNAL(readyRead()), this, SLOT(readData()));
    } else {
        QMessageBox::critical(this, tr("Error"), dongleSerialPort->errorString());

        ui->msgEdit->appendPlainText("打开错误");
        isTestContinue = false;
    }
}

void PressureSensorForm::closeSerialPort() {
    // 启用RTS信号
    dongleSerialPort->setRequestToSend(false);
    // 启用DTR信号
    dongleSerialPort->setDataTerminalReady(false);
    if (dongleSerialPort->isOpen())
        dongleSerialPort->close();
    disconnect(dongleSerialPort, SIGNAL(readyRead()), this, SLOT(readData()));

    ui->refreshButton->setEnabled(true);
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    ui->msgEdit->appendPlainText("串口断开连接");
    refresh_uart_state(0);
}

void PressureSensorForm::handleError(QSerialPort::SerialPortError error) {
    if (error == QSerialPort::ResourceError) {
        closeSerialPort();

        ui->msgEdit->appendPlainText("串口模块拔出");
        refresh_uart_state(0);
        // bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        isTestContinue = false;
    }

    ui->refreshButton->setEnabled(true);
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
}

void PressureSensorForm::on_refreshButton_clicked() {
    ui->comNameCombo->clear();
    foreach (const QSerialPortInfo& info, QSerialPortInfo::availablePorts()) {
        ui->comNameCombo->addItem(info.portName());
    }
}

void PressureSensorForm::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void PressureSensorForm::save_press_test_data_to_csv(const QString& macAddress, const QString& resultunsigned,
                                                     press_calib_data_t cali_result) {
    QDateTime datatime = QDateTime::currentDateTime();
    QString Year_M_D_H = datatime.toString("yyyy-MM-dd_hh") + "_00";

    // 设置文件夹路径为 D 盘
    QString folderPath = "D:/压感校准测试工站/";

    // 创建文件夹（如果不存在）
    QDir dir(folderPath);
    if (!dir.exists("压感校准报告/" + Year_M_D_H)) {
        dir.mkpath("压感校准报告/" + Year_M_D_H);
    }

    // 构建文件路径
    QString filePath = folderPath + "/压感校准报告/" + Year_M_D_H + "/压感测试结果.csv";

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Append)) {
        QTextStream stream(&file);

        // 检查是否是文件的开头，如果是，则写入表头
        if (file.pos() == 0) {
            QStringList headers;
            headers << "时间"
                    << "通道"
                    << "mac地址"
                    << "结果"
                    << "刷头校准系数或错误码"
                    << "模式按键校准系数或错误码"
                    << "电源按键校准系数或错误码"
                    << "温度系数"
                    << "测试结果或失败信息";
            stream << headers.join(",") << "\n";
        }

        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        // 写入数据
        QStringList rowData;
        rowData << timestamp;
        rowData << QString::number(getIndex() + 1);
        rowData << macAddress;
        rowData << result;

        if (result == passValue) {
            // 写入校准系数及测试结果
            rowData << QString::number(cali_result.calib_factor[MODULE_BTH])
                    << QString::number(cali_result.calib_factor[MODULE_MODE_BUTTON])
                    << QString::number(cali_result.calib_factor[MODULE_POWER_BUTTON]);

            // 写入温度系数
            if ((uint32_t)cali_result.temperature[MODULE_BTH] != 0) {
                rowData << QString::number(cali_result.temperature[MODULE_BTH]);
            } else if ((uint32_t)cali_result.temperature[MODULE_MODE_BUTTON] != 0) {
                rowData << QString::number(cali_result.temperature[MODULE_MODE_BUTTON]);
            } else {
                rowData << QString::number(-1);
            }

            // 写入测试结果
            rowData << QString::number(sensor_v[test_chan].test_result[0]);
        } else if (result == failValue) {
            // 写入错误码
            for (int i = 0; i < total_sensor; i++) {
                rowData << QString::number(sensor_v[i].err[0]);
            }
            rowData << QString("测试失败：%1").arg(sensor_v[test_chan].test_result[0]);
        }
        stream << rowData.join(",") << "\n";

        file.close();
        // ui->msgEdit->appendPlainText("文件保存到" + filePath);

        qDebug() << "文件保存到" << filePath;
    } else {
        ui->msgEdit->appendPlainText("文件没关或者其他问题");
        qDebug() << "文件没关或者其他问题";
    }
}

void PressureSensorForm::closeEvent(QCloseEvent*) {
    qDebug() << "已经关闭";
    isTestContinue = false;
}

void PressureSensorForm::clear_display() {
    qDebug() << "clear_display";
    ui->msgEdit->clear();
    ui->log->clear();
}

void PressureSensorForm::on_macInput_returnPressed() {
    clear_display();
    if (!dongleSerialPort->isOpen()) {
        on_connectButton_clicked();
    }
    // 检查是否是mac格式
    QRegularExpression macRegex("^([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}$");
    // 使用正则表达式匹配
    if (!macRegex.match(ui->macInput->text()).hasMatch()) {
        QMessageBox::warning(nullptr, "Warning", QString("%1号机的Mac地址错误").arg(getIndex() + 1));
        return;
    } else {
        macAddress = ui->macInput->text();
        send_mac(macAddress);  // 发送到主界面
        ui->macInput->setEnabled(false);
        stringsn = "";
        state = STATE_IDLE;
        isTestContinue = true;
        // emit gonextfocus();
        emit send_go_next_focus();

        ui->test_result->setText("WAIT");
        ui->test_result->setStyleSheet("font-size: 68px; background-color: #808080; color: black;  border-radius: "
                                       "10px; padding: 10px; text-align: center; ");

        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet("font-size: 68px; background-color: #808080; color: black;  border-radius: 10px; "
                                     "padding: 10px; text-align: center; ");
    }
}

void PressureSensorForm::getMac(QString sn_to_search) {
    QFile file("mac_sn.txt");              // 创建一个文件对象
    if (file.open(QIODevice::ReadOnly)) {  // 打开文件
        QTextStream in(&file);
        while (!in.atEnd()) {                      // 逐行读取文件
            QString line = in.readLine();          // 读取一行
            QStringList fields = line.split(",");  // 将行按照逗号分隔成两个字段
            if (fields.count() >= 2) {             // 至少需要两个字段
                QString sn = fields.at(0);         // 第一个字段是sn
                QString mac = fields.at(1);        // 第二个字段是mac
                if (sn == sn_to_search) {          // 检查是否是待检索的sn
                    ui->macInput->setText(mac);
                    on_macInput_returnPressed();
                    qDebug() << "The corresponding mac is: " << mac;
                    break;
                }
            } else {
                ui->msgEdit->appendPlainText("存在没有逗号分开的" + QString::number(fields.count()) + line);
            }
        }
        file.close();  // 关闭文件
    }
}

void PressureSensorForm::on_getMac_returnPressed() {
    testResultTableInit();
    ui->log->clear();
    ui->msgEdit->clear();
    ui->getMac->setDisabled(1);
    ui->macInput->setDisabled(1);
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
        ui->getMac->setFocus();
        return;
    }

    showlog("正在查询mac地址");
    getMac(ui->getMac->text());             // 文件获取
    processInspection(ui->getMac->text());  // 站前检测
    processGetMesTestValue();               // mes获取
}

void PressureSensorForm::processInspection(QString stringsn) {
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

void PressureSensorForm::processGetMesTestValue() {
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();
        pack.is_hq_send_mac = 1;
        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit getMesTestValue(pack);
    }
}

void PressureSensorForm::on_end_clicked() {
    qDebug() << "on_end_clicked";
    isTestContinue = false;
    if (at->getConnected()) {
        delay_msec(300);
        pb->set_dev_reset();  // 开始复位牙刷
        delay_msec(50);
        ui->msgEdit->appendPlainText("牙刷已复位");
    }

    if (dongleSerialPort->isOpen()) {
        closeSerialPort();
    }

    set_independent_state(STATE_CALIB_RESULT);
    set_fixture_movement(product_model, STATE_SAVE_RESULT, 0);

    ui->macInput->clear();
    ui->getMac->clear();
    ui->macInput->setEnabled(true);
}

void PressureSensorForm::saveDataToLocalFolder(QStringList headers, int32_t* data, uint8_t len, bool appHeader) {
    QDateTime datatime = QDateTime::currentDateTime();
    QString Year_M_D_H = datatime.toString("yyyy-MM-dd_hh") + "_00";

    // 设置文件夹路径为 D 盘
    QString folderPath = "D:/压感校准测试工站/";

    // 创建文件夹（如果不存在）
    QDir dir(folderPath);
    if (!dir.exists("压感校准报告/" + Year_M_D_H)) {
        dir.mkpath("压感校准报告/" + Year_M_D_H);
    }

    // 构建文件路径
    QString filePath = folderPath + "/压感校准报告/" + Year_M_D_H + "/压感数据.csv";

    // 打开文件，如果无法打开则返回
    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning("无法打开文件");
        return;
    }

    // 创建文本流对象
    QTextStream out(&file);

    if (file.pos() == 0 || appHeader) {
        out << headers.join(",") << "\n";
    }

    // 获取当前时间戳
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString timestamp = currentDateTime.toString("yyyy-MM-dd hh:mm:ss");

    // 写入数据
    out << timestamp << "," << getIndex() + 1 << "," << macAddress << "," << state << ",";
    for (uint8_t i = 0; i < len; i++) {
        out << (int16_t)data[i] << ",";
    }
    out << "\n";

    // 关闭文件
    file.close();
}

void PressureSensorForm::product_model_init(QString model) {
    qDebug() << "product_model_init" << model;
    if (model == NULL) {
        qDebug() << "error,model == NULL";
        return;
    }

    if (model == "Y20P") {
        product_model = MODEL_ID_Y20P;
    } else if (model == "F20") {
        product_model = MODEL_ID_F20;
    } else if (model == "U7") {
        product_model = MODEL_ID_U7;
    } else if (model == "Y21") {
        product_model = MODEL_ID_Y21;
    } else if (model == "P30P") {
        product_model = MODEL_ID_P30P;
    }
}

void PressureSensorForm::graph_reset(uint8_t argument) {
    qDebug() << "graph_reset:" << argument;
    if (graph_set_argu == GRAPH_SET_CLOSE) {
        qDebug() << "unused_graph";
        return;
    }

    for (uint8_t chan = 0; chan < graph_adc_vector.size(); chan++) {
        graph_adc_vector[chan]->graph(0)->clearData();
    }
    for (uint8_t chan = 0; chan < graph_value_vector.size(); chan++) {
        graph_value_vector[chan]->graph(0)->clearData();
    }
    counter = 0;
}

void PressureSensorForm::graph_update(FacUploadPresSensor x) {
    if (graph_set_argu <= GRAPH_SET_CLOSE) {
        return;
    }

    for (int i = 0; i < x.sensor_data_count; i++) {
        // qDebug() << "x.sensor_data[i].timestamp;"<<x.sensor_data[i].timestamp;
        int botton_adc = int16_t(x.sensor_data[i].mode_button.adc);
        int botton_value = int16_t(x.sensor_data[i].mode_button.value);

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            switch (sensor_v[channel].para.f_module) {
                case MODULE_BTH:
                    adc_c[channel] = x.sensor_data[i].brush_head.adc;
                    value_c[channel] = x.sensor_data[i].brush_head.value;
                    break;
                case MODULE_MODE_BUTTON:
                    adc_c[channel] = x.sensor_data[i].mode_button.adc;
                    value_c[channel] = x.sensor_data[i].mode_button.value;
                    break;
                case MODULE_POWER_BUTTON:
                    adc_c[channel] = x.sensor_data[i].power_button.adc;
                    value_c[channel] = x.sensor_data[i].power_button.value;
                    break;
            }
        }

        counter += 10;
        for (uint8_t chan = 0; chan < graph_adc_vector.size(); chan++) {
            uint32_t x_min = 0;
            if (counter >= 20000) {
                x_min = counter - 20000;
            }
            graph_adc_vector[chan]->graph(0)->rescaleValueAxis();
            graph_adc_vector[chan]->xAxis->setRange(x_min, counter);

            graph_value_vector[chan]->graph(0)->rescaleValueAxis();
            graph_value_vector[chan]->xAxis->setRange(x_min, counter);
        }

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            switch (sensor_v[channel].para.f_module) {
                case MODULE_BTH:
                    ui->brush_adc->setText("刷头adc：" + QString::number(int16_t(adc_c[channel])));
                    ui->brush_value->setText("刷头压力：" + QString::number(int16_t(value_c[channel])));
                    graph_adc_vector[channel]->graph(0)->setName("刷头ADC值");
                    graph_value_vector[channel]->graph(0)->setName("刷头压力值");
                    break;

                case MODULE_MODE_BUTTON:
                    ui->botton_adc->setText("模式按键adc：" + QString::number(int16_t(adc_c[channel])));
                    ui->botton_value1->setText("模式按键压力：" + QString::number(int16_t(value_c[channel])));
                    graph_adc_vector[channel]->graph(0)->setName("模式按键ADC值");
                    graph_value_vector[channel]->graph(0)->setName("模式按键压力值");
                    break;

                case MODULE_POWER_BUTTON:
                    ui->power_adc->setText("电源按键adc：" + QString::number(int16_t(adc_c[channel])));
                    ui->power_value->setText("电源按键压力：" + QString::number(int16_t(value_c[channel])));
                    graph_adc_vector[channel]->graph(0)->setName("电源按键ADC值");
                    graph_value_vector[channel]->graph(0)->setName("电源按键压力值");
                    break;
            }
            graph_adc_vector[channel]->graph(0)->addData(counter, adc_c[channel]);
            graph_value_vector[channel]->graph(0)->addData(counter, value_c[channel]);
        }

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            // graph_adc_vector[channel]->graph(0)->addData(counter, adc_c[channel]);
            graph_adc_vector[channel]->replot();
            graph_value_vector[channel]->replot();
            // delay_msec(1);
        }
        // ui->botton_adc_diff->setText("adc_diff:" + QString::number(int16_t(sensor_v[calib_chan].para.first_adc[1] -
        // x.sensor_data[i].mode_button.adc)));
    }

    ui->countdown->display((actual_wait_time - countdowntime.elapsed()) / 1000);

    send_frame_rate(QString("%1 ms").arg(transTime.elapsed() / x.sensor_data_count));
    transTime.restart();
}

void PressureSensorForm::save_data(FacUploadPresSensor x) {
    uint8_t data_len = 6;
    for (int i = 0; i < x.sensor_data_count; i++) {
        int32_t* data = new int32_t[data_len];
        adc_c[0] = x.sensor_data[i].brush_head.adc;
        adc_c[1] = x.sensor_data[i].mode_button.adc;

        if (1 || IsSaveFail) {
            QStringList headers;
            headers << "时间戳"
                    << "通道"
                    << "MAC地址"
                    << "状态"
                    << "刷头ADC"
                    << "刷头压力"
                    << "模式按键ADC"
                    << "模式按键压力"
                    << "电源按键ADC"
                    << "电源按键压力";

            data[0] = int32_t(x.sensor_data[i].brush_head.adc);
            data[1] = int32_t(x.sensor_data[i].brush_head.value);
            data[2] = int32_t(x.sensor_data[i].mode_button.adc);
            data[3] = int32_t(x.sensor_data[i].mode_button.value);
            data[4] = int32_t(x.sensor_data[i].power_button.adc);
            data[5] = int32_t(x.sensor_data[i].power_button.value);

            saveDataToLocalFolder(headers, data, data_len, isfirstsavedata);
            delete[] data;
            // saveDataToLocalFolder(int32_t(x.sensor_data[i].brush_head.adc),
            //                       int32_t(x.sensor_data[i].brush_head.value),
            //                       int32_t(x.sensor_data[i].mode_button.adc),
            //                       int32_t(x.sensor_data[i].mode_button.value),
            //                       isfirstsavedata);

            isfirstsavedata = 0;
        }
    }
}

void PressureSensorForm::calib_send_result(void) {
    qDebug() << "发送校准系数";
    repeat_send_ok = 0;
    isStartSendCaliResult = 1;
    if (isStartSendCaliResult) {  // 是否发送出去

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            switch (sensor_v[channel].para.f_module) {
                case MODULE_BTH:
                    cali_result.calib_factor[MODULE_BTH] = sensor_v[channel].calib_result;
                    cali_result.temperature[MODULE_BTH] = sensor_v[channel].temperature;
                    ui->msgEdit->appendPlainText(QString("刷头校准系数：") +
                                                 QString::number(cali_result.calib_factor[MODULE_BTH]));
                    ui->msgEdit->appendPlainText(QString("温度校准系数：") +
                                                 QString::number(cali_result.temperature[MODULE_BTH]));
                    qDebug() << "刷头校准系数：" << cali_result.calib_factor[MODULE_BTH];
                    break;
                case MODULE_MODE_BUTTON:
                    cali_result.calib_factor[MODULE_MODE_BUTTON] = sensor_v[channel].calib_result;
                    cali_result.temperature[MODULE_MODE_BUTTON] = sensor_v[channel].temperature;
                    ui->msgEdit->appendPlainText(QString("模式按键校准系数：") +
                                                 QString::number(cali_result.calib_factor[MODULE_MODE_BUTTON]));
                    if (sensor_v[channel].temperature != 0) {
                        ui->msgEdit->appendPlainText(QString("温度校准系数：") +
                                                     QString::number(cali_result.temperature[MODULE_MODE_BUTTON]));
                    }
                    qDebug() << "模式按键校准系数：" << cali_result.calib_factor[MODULE_MODE_BUTTON];
                    break;
                case MODULE_POWER_BUTTON:
                    cali_result.calib_factor[MODULE_POWER_BUTTON] = sensor_v[channel].calib_result;
                    ui->msgEdit->appendPlainText(QString("电源按键校准系数：") +
                                                 QString::number(cali_result.calib_factor[MODULE_POWER_BUTTON]));
                    qDebug() << "电源按键校准系数：" << cali_result.calib_factor[MODULE_POWER_BUTTON];
                    break;
            }
        }

        isStartSendCaliResult = 0;
        delay_msec(300);
        pb->sendCaliResult(cali_result);
        repeat_send_ok = 1;
        delay_msec(300);
    }
}

void PressureSensorForm::calib_process(FacUploadPresSensor x) {
    if (total_sensor <= 0) {
        qDebug("error,total_sensor:%d", total_sensor);
        return;
    }

    for (int i = 0; i < x.sensor_data_count; i++) {
        int first_runing_suc_num = 0;
        // int calib_suc_num = 0;
        // int adc = int16_t(x.sensor_data[i].brush_head.adc);
        // int value = int16_t(x.sensor_data[i].brush_head.value);
        // int botton_adc = int16_t(x.sensor_data[i].mode_button.adc);
        // int botton_value = int16_t(x.sensor_data[i].mode_button.value);

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            switch (sensor_v[channel].para.f_module) {
                case MODULE_BTH: adc_c[channel] = x.sensor_data[i].brush_head.adc; break;
                case MODULE_MODE_BUTTON: adc_c[channel] = x.sensor_data[i].mode_button.adc; break;
                case MODULE_POWER_BUTTON: adc_c[channel] = x.sensor_data[i].power_button.adc; break;
            }
        }

        isStartcali = 1;  // 开始测试

        if (first_start_test) {
            for (int i = 0; i < total_sensor; i++) {
                // qDebug() << "i" << i << "gs32SensorFlag" << sensor_v[i].gs32SensorFlag;
                if (sensor_v[i].gs32SensorFlag < 2) {
                    sensor_v[i].ndt_sensor_cali_process(x.sensor_data_count, adc_c + i);  // 需要跑一hui
                } else if (sensor_v[i].gs32SensorFlag == 2) {
                    first_runing_suc_num++;
                }
            }
            if (first_runing_suc_num == total_sensor) {
                first_start_test = 0;
                for (int i = 0; i < total_sensor; i++) {
                    sensor_v[i].para.first_adc = adc_c[i];
                    qDebug() << QString("first_adc[%1]").arg(i) << sensor_v[i].para.first_adc;
                }
            }
        }
#if 1  // Avoiding_the_influence_of_BTH // 避免刷头给按键校准造成干扰
        if (sensor_v[calib_chan].para.f_module == MODULE_MODE_BUTTON) {
            if (x.sensor_data[i].brush_head.value < 50) {
                if ((adc_c[calib_chan] - sensor_v[calib_chan].para.first_adc) >
                        sensor_v[calib_chan].para.adc_threshold &&
                    !start_calib_channel[calib_chan] && sensor_v[calib_chan].para.first_adc != 0) {
                    brush_before_calib_count++;
                    if (brush_before_calib_count > sensor_v[calib_chan].para.count_threshold) {
                        start_calib_channel[calib_chan] = 1;
                        brush_before_calib_count = 0;
                    }
                }
            }
        } else {
            if ((adc_c[calib_chan] - sensor_v[calib_chan].para.first_adc) > sensor_v[calib_chan].para.adc_threshold &&
                !start_calib_channel[calib_chan] && sensor_v[calib_chan].para.first_adc != 0) {
                brush_before_calib_count++;
                if (brush_before_calib_count > sensor_v[calib_chan].para.count_threshold) {
                    start_calib_channel[calib_chan] = 1;
                    brush_before_calib_count = 0;
                }
            }
        }
#else
        if ((adc_c[calib_chan] - sensor_v[calib_chan].para.first_adc) > sensor_v[calib_chan].para.adc_threshold &&
            !start_calib_channel[calib_chan] && sensor_v[calib_chan].para.first_adc != 0) {
            brush_before_calib_count++;
            if (brush_before_calib_count > sensor_v[calib_chan].para.count_threshold) {
                start_calib_channel[calib_chan] = 1;
                brush_before_calib_count = 0;
            }
        }
#endif

        if (sensor_v[calib_chan].gs32SensorFlag == 1) {
            // sensor_v[calib_chan].ndt_sensor_cali_process(x.sensor_data_count, adc_c + calib_chan);
        }

        if (sensor_v[calib_chan].gs32SensorFlag == 2 && start_calib_channel[calib_chan] &&
            sensor_v[calib_chan].calib_status != CALIB_NOCALIB) {
            sensor_v[calib_chan].ndt_sensor_cali_process(x.sensor_data_count, adc_c + calib_chan);
        }

        if (sensor_v[calib_chan].gs32SensorFlag == 3) {
            unsigned short* cali_ok =
                sensor_v[calib_chan].ndt_sensor_cali_process(x.sensor_data_count, adc_c + calib_chan);
            sensor_v[calib_chan].calib_status = CALIB_SUC;

            if (cali_ok[0] != 0) {
                sensor_v[calib_chan].calib_result = cali_ok[0];
                sensor_v[calib_chan].temperature = x.sensor_data[i].temperature;
                sensor_v[calib_chan].sensor_cali_set(0);
            }
            if (calib_chan + 1 == total_sensor) {
                isCaliOk = 1;  // 校准完成的标志
            }
        }

        // 在函数内使用标志，确保每个条件只运行一次
        if (sensor_v[calib_chan].gs32SensorFlag == 1 && !donotmoveFlag) {
            // ui->msgEdit->appendPlainText(donotmove);
            donotmoveFlag = true;
        }

        if (sensor_v[calib_chan].gs32SensorFlag == 2 && !brushPlacingFlag && is_get_ready_command) {
            brushPlacingFlag = true;
        }

        if (sensor_v[calib_chan].gs32SensorFlag == 3 && !packWeightsFlag) {
            packWeightsFlag = true;
        }

        if (sensor_v[calib_chan].gs32SensorFlag == 4 && !bottonPlacingFlag) {
            ui->msgEdit->appendPlainText(botton_placing_200_weights);
            bottonPlacingFlag = true;
        }

        if (sensor_v[calib_chan].gs32SensorFlag == 5 && !caliResultOkFlag) {
            ui->msgEdit->appendPlainText(cali_result_ok);
            caliResultOkFlag = true;
        }

        if (sensor_v[calib_chan].gs32SensorFlag == 6 && !caliResultFailFlag) {
            ui->msgEdit->appendPlainText(cali_result_fail);
            caliResultFailFlag = true;

            ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_err[0] +
                                         QString::number(sensor_v[calib_chan].err[0]));
        }
    }
}

void appendFormattedText(QTextEdit* textEdit, const QString& text, const QColor& textColor) {
    // 获取当前文本光标
    QTextCursor cursor = textEdit->textCursor();

    // 启用自动换行
    textEdit->setWordWrapMode(QTextOption::WordWrap);

    // 清空QTextEdit内容
    textEdit->clear();

    static uint8_t colo = 0;
    QColor backgroundColor = QColor("#00FF00");  // green
    switch (colo) {
        case 0:
            backgroundColor = QColor("#00FF00");
            colo++;
            break;

        case 1:
            backgroundColor = QColor("#00FFFF");
            colo++;
            break;

        case 2:
            backgroundColor = QColor("#FFFF00");
            colo = 0;
            break;

        default: backgroundColor = QColor("#00FF00"); break;
    }

    bool bold = true;

    // 字体大小
    int fontSize = 23;

    // 创建一个文本格式对象并设置颜色、背景颜色、字体大小和加粗
    QTextCharFormat format;
    format.setForeground(textColor);
    format.setBackground(backgroundColor);
    format.setFontPointSize(fontSize);
    if (bold) {
        format.setFontWeight(QFont::Bold);
    } else {
        format.setFontWeight(QFont::Normal);
    }

    // 设置文本光标的字符格式
    cursor.setCharFormat(format);

    // 插入带格式的文本
    cursor.insertText(text);
    cursor.insertText("\n");

    // 将光标格式重置为默认格式
    cursor.setCharFormat(QTextCharFormat());
}

void PressureSensorForm::test_process(FacUploadPresSensor x) {
    if (total_sensor <= 0) {
        qDebug("error,total_sensor:%d", total_sensor);
        return;
    }

    for (int i = 0; i < x.sensor_data_count; i++) {
        // int first_runing_suc_num = 0;
        // int calib_suc_num = 0;
        // int adc = int16_t(x.sensor_data[i].brush_head.adc);
        // int value = int16_t(x.sensor_data[i].brush_head.value);
        // int botton_adc = int16_t(x.sensor_data[i].mode_button.adc);
        // int botton_value = int16_t(x.sensor_data[i].mode_button.value);

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            switch (sensor_v[channel].para.f_module) {
                case MODULE_BTH:
                    adc_c[channel] = x.sensor_data[i].brush_head.adc;
                    value_c[channel] = x.sensor_data[i].brush_head.value;
                    break;
                case MODULE_MODE_BUTTON:
                    adc_c[channel] = x.sensor_data[i].mode_button.adc;
                    value_c[channel] = x.sensor_data[i].mode_button.value;
                    break;
                case MODULE_POWER_BUTTON:
                    adc_c[channel] = x.sensor_data[i].power_button.adc;
                    value_c[channel] = x.sensor_data[i].power_button.value;
                    break;
            }
        }

        switch (sensor_v[test_chan].para.f_module) {
            case MODULE_BTH:
                if (sensor_v[test_chan].test_status == TEST_START) {
                    static uint8_t lock = 0;
                    if (pb->getisDevintowhitemode()) {
                        sensor_v[test_chan].test_status = TEST_BTH_SHAKE;
                        // #if 1
                        //     sensor_v[test_chan].test_status = TEST_BTH_NORMAL;
                        //     appendFormattedText(ui->tip, sensor_v[test_chan].ui_msg_test[0], QColor("black"));
                        // #endif
                        set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                        delay_msec(200);
                        pb->set_brush_control(1);
                        appendFormattedText(ui->tip, sensor_v[test_chan].ui_msg_test[0], QColor("black"));
                        ui->msgEdit->appendPlainText(sensor_v[test_chan].ui_msg_test[0]);
                    } else if (lock == 0) {
                        lock = 1;
                        pb->set_device_mode(3);  // 清洁模式
                        // pb->setDevintowhitemode();   // 进入亮白
                        ui->msgEdit->appendPlainText("TEST_START");
                        delay_msec(200);
                        lock = 0;
                    }
#if 0  // Y20_PRESS_TEST
                    ui->msgEdit->appendPlainText("放上350g砝码");   
                    countdowntime.start();
#endif
                }
                // else if (sensor_v[test_chan].test_status == TEST_BTH_END) {
                //     sensor_v[test_chan].test_status = TEST_BTH_OVERPRESS;
                // }
                break;

            case MODULE_MODE_BUTTON:
            case MODULE_POWER_BUTTON:
#if 0  // USE_KEY_CLICK_TEST // 按键测试 0:使用按键是否响应的方案 1：使用200g砝码值比对的方案
                if (sensor_v[test_chan].test_status == TEST_START) {
                    sensor_v[test_chan].test_status = TEST_KET_NORMAL;
                    ui->msgEdit->appendPlainText(sensor_v[test_chan].ui_msg_test[0]);
                    qDebug() << "TEST_KET_NORMAL";
                }
#else
                if (sensor_v[test_chan].test_status == TEST_START) {
                    sensor_v[test_chan].test_status = TEST_KET_NO_CLICK;
                    pb->get_button_state(1);
                    delay_msec(500);
                    qDebug() << "set_fixture_movement:key_test";
                    ui->msgEdit->appendPlainText(sensor_v[test_chan].ui_msg_test[0]);
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                    qDebug() << "TEST_KET_NO_CLICK";
                }
#endif
                break;
        }

        if (sensor_v[test_chan].test_status == TEST_BTH_SHAKE) {
            if (value_c[test_chan] > 50 - 10 && value_c[test_chan] < 50 + 10)  // 数据准确
            {
                qDebug("TEST_BTH_SHAKE:%d", sensor_v[test_chan].para.current_count);
                sensor_v[test_chan].para.current_count++;
                if (sensor_v[test_chan].para.current_count >= 200) {
                    sensor_v[test_chan].para.current_count = 0;
                    sensor_v[test_chan].test_status = TEST_BTH_NORMAL;
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                    appendFormattedText(ui->tip, sensor_v[test_chan].ui_msg_test[1], QColor("black"));
                    ui->msgEdit->appendPlainText(sensor_v[test_chan].ui_msg_test[1]);
                }
            }
            sensor_v[test_chan].test_result[0] = 50;
        } else if (sensor_v[test_chan].test_status == TEST_BTH_NORMAL) {
            if (value_c[test_chan] > 350 - 0.1 * 350 && value_c[test_chan] < 350 + 0.1 * 350)  // 数据准确
            {
                qDebug("TEST_BTH_NORMAL:%d", sensor_v[test_chan].para.current_count);
                sensor_v[test_chan].para.current_count++;

                if (sensor_v[test_chan].para.current_count >= 200) {
                    sensor_v[test_chan].para.current_count = 0;
                    sensor_v[test_chan].test_status = TEST_BTH_OVERPRESS;
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                    appendFormattedText(ui->tip, sensor_v[test_chan].ui_msg_test[2], QColor("black"));
                    ui->msgEdit->appendPlainText(sensor_v[test_chan].ui_msg_test[2]);
                }
            } else if (value_c[test_chan] > 400) {  // 不该过压的时候过压
                sensor_v[test_chan].para.error_count++;
                if (sensor_v[test_chan].para.error_count >= 80) {
                    sensor_v[test_chan].para.error_count = 0;
                    sensor_v[test_chan].test_status = TEST_FAIL;
                }
            }
            sensor_v[test_chan].test_result[0] = 350;

#if 0  // Y20_PRESS_TEST
            if (value_c[test_chan] < 5)   // 未过压
            {
                qDebug("TEST_BTH_NORMAL:%d",sensor_v[test_chan].para.current_count);
                sensor_v[test_chan].para.current_count++;
                if (sensor_v[test_chan].para.current_count >= (actual_wait_time / 10 * 8/10))
                {
                    sensor_v[test_chan].para.current_count = 0;
                    sensor_v[test_chan].test_status = TEST_BTH_OVERPRESS;
                    ui->msgEdit->appendPlainText("放上450g砝码");

                    countdowntime.restart();
                }
            } else if (value_c[test_chan] >= 5) {
                sensor_v[test_chan].para.current_count = 0;
                sensor_v[test_chan].test_status = TEST_FAIL;
            }
#endif
        } else if (sensor_v[test_chan].test_status == TEST_BTH_OVERPRESS) {
            if (value_c[test_chan] > 450 - 0.1 * 450 && value_c[test_chan] < 450 + 0.1 * 450)  // 数据准确
            {
                qDebug("TEST_BTH_OVERPRESS:%d", sensor_v[test_chan].para.current_count);
                sensor_v[test_chan].para.current_count++;
                if (sensor_v[test_chan].para.current_count >= 300) {
                    sensor_v[test_chan].para.current_count = 0;
                    sensor_v[test_chan].test_status = TEST_SUC;
                }
            }
            sensor_v[test_chan].test_result[0] = 450;
#if 0  // Y20_PRESS_TEST
            if (value_c[test_chan] >= 5)   // 过压
            {
                qDebug("TEST_BTH_OVERPRESS:%d",sensor_v[test_chan].para.current_count);
                sensor_v[test_chan].para.current_count++;
                if (sensor_v[test_chan].para.current_count >= 200)
                {
                    sensor_v[test_chan].para.current_count = 0;
                    sensor_v[test_chan].test_status = TEST_SUC;
                }
            }
#endif
        } else if (sensor_v[test_chan].test_status == TEST_KET_NORMAL) {
            qDebug("TEST_KET_NORMAL:%d", sensor_v[test_chan].para.current_count);
            if (value_c[test_chan] > 200 - 30 && value_c[test_chan] < 200 + 30) {
                sensor_v[test_chan].para.current_count++;
                if (sensor_v[test_chan].para.current_count >= 200) {
                    sensor_v[test_chan].test_result[0] = value_c[test_chan];
                    sensor_v[test_chan].para.current_count = 0;
                    sensor_v[test_chan].test_status = TEST_SUC;
                }
            }
        } else if (sensor_v[test_chan].test_status == TEST_KET_NO_CLICK) {
            qDebug("TEST_KET_NO_CLICK:%d", sensor_v[test_chan].para.current_count);
// 无视电容触发
#if 1
            if (sensor_v[test_chan].para.no_click_max < value_c[test_chan]) {
                sensor_v[test_chan].para.no_click_max = value_c[test_chan];
            }
            if (sensor_v[test_chan].para.no_click_max > 200) {
                sensor_v[test_chan].para.button_state = 1;
            }
#endif
            if (sensor_v[test_chan].send_state == 1) {
                sensor_v[test_chan].send_state = 0;
                if (sensor_v[test_chan].para.button_state == 0) {
                    // sensor_v[test_chan].test_result[0] = 100;
                    sensor_v[test_chan].para.button_state = 0;
                    sensor_v[test_chan].test_status = TEST_KET_CLICK;
                    ui->msgEdit->appendPlainText(sensor_v[test_chan].ui_msg_test[1]);
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                } else if (sensor_v[test_chan].para.button_state == 1) {
                    sensor_v[test_chan].test_status = TEST_FAIL;
                    ui->msgEdit->appendPlainText("测试失败：100g按键触发");
                }
            }
        } else if (sensor_v[test_chan].test_status == TEST_KET_CLICK) {
            qDebug("TEST_KET_CLICK:%d", sensor_v[test_chan].para.current_count);
// 无视电容触发
#if 1
            if (sensor_v[test_chan].para.click_max < value_c[test_chan]) {
                sensor_v[test_chan].para.click_max = value_c[test_chan];
            }
            if (sensor_v[test_chan].para.click_max > 250) {
                sensor_v[test_chan].para.button_state = 1;
            }
#endif
            if (sensor_v[test_chan].send_state == 1) {
                sensor_v[test_chan].send_state = 0;
                if (sensor_v[test_chan].para.button_state == 1) {
                    // sensor_v[test_chan].test_result[0] = 0;
                    sensor_v[test_chan].para.button_state = 0;
                    sensor_v[test_chan].test_status = TEST_SUC;
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                } else if (sensor_v[test_chan].para.button_state == 0) {
                    sensor_v[test_chan].test_status = TEST_FAIL;
                    ui->msgEdit->appendPlainText("测试失败：300g按键未触发");
                }
            }
        }
    }
}

void PressureSensorForm::getPressSensorData(FacUploadPresSensor x) {
    graph_update(x);  // 图像更新
    save_data(x);
    calib_process(x);
    test_process(x);
}

void PressureSensorForm::getButtonState(FacButtonState x) {
    qDebug() << "getButton_State";
    for (int i = 0; i < x.button_state_count; i++) {
        if (x.button_state[0].command_data.power_button.button_state_now == ButtonState_PRESSED) {
            sensor_v[test_chan].para.button_state = 1;
        }
    }
}

void PressureSensorForm::delay_msec(unsigned int msec) {
    QEventLoop loop;                                // 定义一个新的事件循环
    QTimer::singleShot(msec, &loop, SLOT(quit()));  // 创建单次定时器，槽函数为事件循环的退出函数
    loop.exec();  // 事件循环开始执行，程序会卡在这里，直到定时时间到，本循环被退出
}

void PressureSensorForm::refresh_sn(FacDevInfo data) {
    stringsn = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    qDebug() << "dev_info" << data.dev_info[0].value_item.tail_sn;
    qDebug() << "stringsn" << stringsn;
    ui->msgEdit->appendPlainText("芯片存储的尾盖sn:" + stringsn);
}

void PressureSensorForm::solve_mes_sucess(const int mechines) {
    if (mechines == getIndex()) {
        ui->msgEdit->appendPlainText("mes操作成功");
        qDebug() << "mes操作成功";
        mes_set_ok = 1;
    }
}
void PressureSensorForm::solve_mes_data(const int mechines, QString msgEdit) {
    if (mechines == getIndex()) {
        ui->msgEdit->appendPlainText("MES:报错信息:" + msgEdit);
        isTestContinue = false;  // 结束
        ui->msgEdit->appendPlainText("停止运行");
        at->resetConnected();
        ui->macInput->clear();
        ui->getMac->clear();

        ui->macInput->setFocus();
        ui->macInput->setDisabled(0);
    }
}

void PressureSensorForm::reset_all() {
    qDebug() << "reset_all";
    // send_start_command(COMMAND_ID_BASE, 0);   // 复位
    getaskbarcode = 0;
    is_get_ready_command = 0;
    isCaliOk = 0;               // 是否校准完成
    isStartSendCaliResult = 0;  // 是否开始发送校验结果
    isovertime = 0;             // 超时标志位

    data_200_true_count = 0;
    data_250_true_count = 0;
    data_under_pressure_error_count = 0;
    data_350_true_count = 0;
    data_over_pressure_error_count = 0;

    counter = 0;
    graph_reset(0);
    data_200_true_count = 0;
    data_250_true_count = 0;
    data_350_true_count = 0;
    donotmoveFlag = false;
    brushPlacingFlag = false;
    packWeightsFlag = false;
    bottonPlacingFlag = false;
    caliResultOkFlag = false;
    caliResultFailFlag = false;
    calib_chan = 0;
    test_chan = 0;
    first_start_test = 1;  // 只进行一次
    isfirstsavedata = 1;   // 是否第一次保存压感数据
    TEST_STATE = 0;
    is_calib_suc = 0;

    if (!sensor_v.empty()) {
        qDebug() << "sensor_v contains" << sensor_v.size() << "elements.";

        // 清空 sensor_v
        sensor_v.clear();
        qDebug() << "sensor_v has been cleared.";
    }
    // for(int i = 0; i < total_sensor; i++){
    //     sensor_v[i].sensor_cali_reset();
    //     sensor_v[i].err[0] = 0;
    //     sensor_v[i].err[1] = 0;
    // }

    pb->reset_all_pb();
    at->resetConnected();
    ui->log->clear();
    isStartcali = 0;

    memset(&cali_result, 0, sizeof(press_calib_data_t));
    memset(&LastCali, 0, sizeof(press_calib_data_t));
    memset(&start_calib_channel, 0, sizeof(start_calib_channel));  // 是否开始刷头200校准

    emit operator_instruct(getIndex(), 0);

    set_independent_state(STATE_INVALID);
    at->sendMac(ui->macInput->text());
    ui->msgEdit->appendPlainText("开始测试");
    ui->msgEdit->appendPlainText("请扫描二维码");
}

void PressureSensorForm::set_fixture_movement(MODEL_ID_E model, State state, int argument) {
    qDebug() << "set_movement model" << model << state << argument;
    switch (model) {
        case MODEL_ID_Y20P: Y20P_fixture(state, argument); break;

        case MODEL_ID_F20: F20_fixture(state, argument); break;

        case MODEL_ID_U7: U7_fixture(state, argument); break;

        case MODEL_ID_P30P:
            // 治具指令跟U7一致
            U7_fixture(state, argument);
            break;
    }
}

void PressureSensorForm::Y20P_fixture(State state, int argument) {
    switch (state) {
        case STATE_CALIB_CH_X:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_BTH:

                    set_independent_state(STATE_CALIB_CH0);
                    send_start_command(COMMAND_ID_BTH_DOWN_200, 0);
                    break;

                case MODULE_MODE_BUTTON: send_start_command(COMMAND_ID_KEY_DOWN_200, 0); break;
            }
            break;

        case STATE_CALIB_CH_X_RESULT:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_BTH:

                    set_independent_state(STATE_CALIB_CH1);
                    send_start_command(COMMAND_ID_BTH_UP, 0);
                    break;

                case MODULE_MODE_BUTTON: send_start_command(COMMAND_ID_KEY_UP, 0); break;
            }

            if (calib_chan + 1 == total_sensor) {
                send_start_command(COMMAND_ID_FIXED_BLOCK_UP, 0);
                send_start_command(COMMAND_ID_TRAY_OUT, 0);
            }
            break;

        case STATE_WATI_ASKQR:
#if USE_CALIB_FIXTURE
            send_start_command(COMMAND_ID_TRAY_IN, 0);
            send_start_command(COMMAND_ID_FIXED_BLOCK_DOWN, 0);
#endif
            break;

        case STATE_SAVE_RESULT:
            set_independent_state(STATE_CALIB_RESULT);

            // calib
#if USE_CALIB_FIXTURE
            send_start_command(COMMAND_ID_BTH_UP, 0);
            send_start_command(COMMAND_ID_KEY_UP, 0);
            send_start_command(COMMAND_ID_FIXED_BLOCK_UP, 0);
            send_start_command(COMMAND_ID_TRAY_OUT, 0);
#endif

            // test
#if USE_TEST_FIXTURE
            send_start_command(COMMAND_ID_BTH_PRESS_UP, 0);
            send_start_command(COMMAND_ID_RESULT, 0);
#endif
            break;

        case STATE_TEST_CH_X:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_BTH:
                    switch (sensor_v[test_chan].test_status) {
                        case TEST_BTH_SHAKE: send_start_command(COMMAND_ID_BTH_PRESS_50, 0); break;

                        case TEST_BTH_NORMAL: send_start_command(COMMAND_ID_BTH_PRESS_350, 0); break;

                        case TEST_BTH_OVERPRESS: send_start_command(COMMAND_ID_BTH_PRESS_450, 0); break;
                    }
                    break;
            }
            break;

        case STATE_TEST_CH_X_RESULT:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_BTH: send_start_command(COMMAND_ID_BTH_PRESS_UP, 0); break;
            }
            break;
    }
}

void PressureSensorForm::F20_fixture(State state, int argument) {
    switch (state) {
        case STATE_WATI_ASKQR:
            send_start_command(COMMAND_ID_F20_FIXED, 0);
            delay_msec(2000);
            break;

        case STATE_CALIB_CH_X:
        case STATE_TEST_CH_X:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_BTH: set_independent_state(STATE_CALIB_CH0); break;

                case MODULE_MODE_BUTTON:
                    send_start_command(COMMAND_ID_KEY_SWITCH_MODE, 0);
                    delay_msec(50);
                    send_start_command(COMMAND_ID_KEY_DOWN, 0);
                    break;

                case MODULE_POWER_BUTTON:
                    send_start_command(COMMAND_ID_KEY_SWITCH_POWER, 0);
                    delay_msec(50);
                    send_start_command(COMMAND_ID_KEY_DOWN, 0);
                    break;
            }
            break;

        case STATE_CALIB_CH_X_RESULT:
        case STATE_TEST_CH_X_RESULT:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_BTH: set_independent_state(STATE_CALIB_CH1); break;

                case MODULE_MODE_BUTTON:
                    send_start_command(COMMAND_ID_KEY_UP, 0);
                    delay_msec(500);  // 等待按键砝码抬起
                    break;
            }

            // 所有通道校准完成，退出所有治具
            if (calib_chan + 1 == total_sensor) {
                send_start_command(COMMAND_ID_KEY_UP, 0);
                delay_msec(500);  // 等待按键砝码抬起
            }
            break;

        case STATE_SAVE_RESULT:
            set_independent_state(STATE_CALIB_RESULT);

            send_start_command(COMMAND_ID_KEY_UP, 0);
            // system_cmd("none", SYSTEM_COMMAND_ZERO);
            delay_msec(500);
            send_start_command(COMMAND_ID_F20_UNFIXED, 0);
            break;

        default: break;
    }
}

void PressureSensorForm::U7_fixture(State state, int argument) {
    switch (state) {
        case STATE_CALIB_CH_X:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_MODE_BUTTON:
                case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_O, 0); break;
            }
            break;

        case STATE_CALIB_CH_X_RESULT:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_MODE_BUTTON:
                case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
            }
            break;

        case STATE_TEST_CH_X:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_MODE_BUTTON:
                case MODULE_POWER_BUTTON:
                    switch (sensor_v[test_chan].test_status) {
                        case TEST_KET_NO_CLICK:
                            send_start_command(COMMAND_ID_FAMA_100_O, 0);
                            delay_msec(500);
                            send_start_command(COMMAND_ID_FAMA_100_C, 0);
                            delay_msec(500);
                            sensor_v[test_chan].send_state = 1;
                            break;

                        case TEST_KET_CLICK:
                            send_start_command(COMMAND_ID_FAMA_300_O, 0);
                            delay_msec(500);
                            send_start_command(COMMAND_ID_FAMA_300_C, 0);
                            delay_msec(500);
                            sensor_v[test_chan].send_state = 1;
                            break;
                    }
                    break;
            }
            break;

        case STATE_TEST_CH_X_RESULT:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_MODE_BUTTON:
                case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
            }
            break;

        case STATE_SAVE_RESULT:
            switch (sensor_v[argument].para.f_module) {
                case MODULE_MODE_BUTTON:
                case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
            }
            break;

        default: break;
    }
}

void PressureSensorForm::ui_msg_show(MODEL_ID_E model, State state, int argument) {
    switch (model) {
        case MODEL_ID_Y20P:
            if (state == STATE_CALIB_CH_X) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[1]);
            } else if (state == STATE_CALIB_CH_X_RESULT) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[2]);
            }

            break;

        case MODEL_ID_F20:
            if (state == STATE_CALIB_CH_X) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[1]);
            } else if (state == STATE_CALIB_CH_X_RESULT) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[2]);
            }
            break;

        case MODEL_ID_U7:
            if (state == STATE_CALIB_CH_X) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[1]);
            } else if (state == STATE_CALIB_CH_X_RESULT) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[2]);
            } else if (state == STATE_TEST_CH_X) {
                ui->msgEdit->appendPlainText(QString("测试通道%1:按键").arg(test_chan));
            } else if (state == STATE_TEST_CH_X_RESULT) {
                ui->msgEdit->appendPlainText(QString("通道%1：按键已测试完毕").arg(test_chan));
            } else if (state == STATE_CALIB_ALL_RESULT) {
                ui->msgEdit->appendPlainText("测试结束");
            }
            break;

        case MODEL_ID_Y21:
            if (state == STATE_CALIB_CH_X) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[1]);
            } else if (state == STATE_CALIB_CH_X_RESULT) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[2]);
            } else if (state == STATE_TEST_CH_X) {
                ui->msgEdit->appendPlainText(QString("测试通道%1:按键").arg(test_chan));
            } else if (state == STATE_TEST_CH_X_RESULT) {
                ui->msgEdit->appendPlainText(QString("通道%1：按键已测试完毕").arg(test_chan));
            } else if (state == STATE_CALIB_ALL_RESULT) {
                ui->msgEdit->appendPlainText("测试结束");
            }
            break;

        case MODEL_ID_P30P:
            if (state == STATE_CALIB_CH_X) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[1]);
            } else if (state == STATE_CALIB_CH_X_RESULT) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[2]);
            } else if (state == STATE_TEST_CH_X) {
                ui->msgEdit->appendPlainText(QString("测试通道%1:按键").arg(test_chan));
            } else if (state == STATE_TEST_CH_X_RESULT) {
                ui->msgEdit->appendPlainText(QString("通道%1：按键已测试完毕").arg(test_chan));
            } else if (state == STATE_CALIB_ALL_RESULT) {
                ui->msgEdit->appendPlainText("测试结束");
            }
            break;

        default: break;
    }
}

void PressureSensorForm::startTask() {
    if (isTestContinue) {
        // ui->test_time->display(TestTime.elapsed() / 1000);
        switch (state) {
            case STATE_IDLE:  // 复位一切
                reset_all();
                calib_vector_init(product_model);
                qDebug() << "sizeof(sensor_v)" << sensor_v.size();
                // at->sendMac(ui->macInput->text());  // 发送mac地址
                showlog(ui->macInput->text());
                state = STATE_WATI_CONNECT;
                break;

            case STATE_WATI_CONNECT:  // 设置禁止休眠
                if (at->getConnected()) {
                    ui->msgEdit->appendPlainText("蓝牙连接成功");
                    emit refresh_ble_state(1);
                    delay_msec(200);
                    pb->get_base_info();  // 获取设备信息
                    delay_msec(200);
                    pb->set_forbid_sleep(FacSwitch_OPEN);
                    delay_msec(200);
                    ui->msgEdit->appendPlainText("已发送禁止休眠");
                    state = STATE_DISABLE_SLEEP_CALIB;
                } else {
                    emit refresh_ble_state(0);
                }
                break;

            case STATE_DISABLE_SLEEP_CALIB:  // 禁止休眠
                delay_msec(100);
                if (pb->getDisableSleep()) {
                    delay_msec(200);
                    ui->msgEdit->appendPlainText("已进入禁止休眠");
                    pb->set_press_collect_param(FacSwitch_START);
                    delay_msec(200);
                    state = STATE_WATI_ASKQR;
                } else {
                    delay_msec(100);
                    pb->set_forbid_sleep(FacSwitch_OPEN);
                    delay_msec(200);
                    ui->msgEdit->appendPlainText("再次发送禁止休眠");
                }
                break;

            case STATE_WATI_ASKQR:
                if (get_independent_state() == STATE_CALIB_START) {
                    set_independent_state(STATE_CALIB_STATIC_WAIT);
                    set_fixture_movement(product_model, STATE_WATI_ASKQR, 0);
                    ui->msgEdit->appendPlainText("等待治具就绪");
                    state = STATE_WAIT_STATIC;
                } else if (get_independent_state() == STATE_INVALID) {
                    set_independent_state(STATE_WAIT_START);
                    set_independent_state(STATE_CALIB_START);
                    emit operator_instruct(getIndex(), 1);
                }
                break;
                break;

            case STATE_WAIT_STATIC:  // 收集无负载时的数据
                qDebug() << "pb->getCollectParam()" << pb->getCollectParam() << "isStartcali" << isStartcali;
#if IS_INDEPENDENT
                // set_independent_state(STATE_CALIB_STATIC_START);
#endif
                if (pb->getCollectParam() && isStartcali && get_independent_state() == STATE_CALIB_STATIC_START) {
                    delay_msec(200);
                    if (only_mode == "BTH_ONLY" || only_mode == "KEY_ONLY") {
                        pb->get_cali_result();
                    }

                    ui->msgEdit->appendPlainText("已设置压感采集参数");

                    if (function_switch == FUNCTION_CALIB_TEST || function_switch == FUNCTION_CALIB) {
                        ui->msgEdit->appendPlainText(donotmove);
                        for (int i = 0; i < sensor_v.size(); i++) {
                            sensor_v[i].sensor_cali_set(1);
                        }
                        state = STATE_CALIB_CH_X;
                    } else if (function_switch == FUNCTION_TEST) {
                        state = STATE_TEST_CH_X;
#if Y20_Pro_PRESS_TEST  // Y20pro测试前需要校准系数阈值比对
                        pb->get_cali_result();
                        state = STATE_TEST_PARM_LIMIT;
#endif
                    } else {
                        state = STATE_CALIB_CH_X;
                    }
                }
                break;

            case STATE_CALIB_CH_X:  //开始校准,治具动作

                if (sensor_v[calib_chan].gs32SensorFlag == 2 && sensor_v[calib_chan].calib_status != CALIB_NOCALIB) {
                    set_fixture_movement(product_model, STATE_CALIB_CH_X, calib_chan);
                    ui_msg_show(product_model, STATE_CALIB_CH_X, calib_chan);
                    // waittime->setInterval(measure_wait_time);   // 超时判断计时开始
                    // waittime->start();
                    qDebug() << "waittime->start();";
                    state = STATE_CALIB_CH_X_RESULT;
                } else if (sensor_v[calib_chan].calib_status == CALIB_NOCALIB) {
                    if (LastCali.calib_factor[sensor_v[calib_chan].para.f_module] != 0) {
                        switch (sensor_v[calib_chan].para.f_module) {
                            case MODULE_BTH:
                                sensor_v[calib_chan].calib_result = LastCali.calib_factor[MODULE_BTH];
                                sensor_v[calib_chan].temperature = LastCali.temperature[MODULE_BTH];
                                break;

                            case MODULE_MODE_BUTTON:
                                sensor_v[calib_chan].calib_result = LastCali.calib_factor[MODULE_MODE_BUTTON];
                                break;

                            case MODULE_POWER_BUTTON:
                                sensor_v[calib_chan].calib_result = LastCali.calib_factor[MODULE_POWER_BUTTON];
                                break;
                        }
                        if (pb->get_is_save_press_cali_data()) {
                            pb->reset_all_pb();
                            is_calib_suc = 0;
                        }
                        state = STATE_CALIB_CH_X_RESULT;
                    }
                }
                break;

            case STATE_CALIB_CH_X_RESULT:  //开始校准，等待完成

                if (sensor_v[calib_chan].calib_status == CALIB_SUC ||
                    sensor_v[calib_chan].calib_status == CALIB_NOCALIB) {
                    ui_msg_show(product_model, STATE_CALIB_CH_X_RESULT, calib_chan);
                    ui->log->appendPlainText(QString("通道%1已校准完毕").arg(calib_chan));
                    set_fixture_movement(product_model, STATE_CALIB_CH_X_RESULT, calib_chan);
                    if (calib_chan + 1 < total_sensor) {
                        calib_chan++;
                        state = STATE_CALIB_CH_X;
                        ui->log->appendPlainText(QString("通道%1开始校准").arg(calib_chan));
                    } else {
                        state = STATE_CHECK_CAIL_RESULT;
                        ui->msgEdit->appendPlainText("所有通道校准完毕");
                    }
                    waittime->stop();
                    qDebug() << "ime->stop();";
                } else if (sensor_v[calib_chan].gs32SensorFlag == 6) {
                    pb->set_press_collect_param(FacSwitch_STOP);
                    delay_msec(200);
                    ui->msgEdit->appendPlainText("压感校准失败");
                    waittime->stop();
                    qDebug() << "stopime->stop();";
                    result = failValue;
                    state = STATE_SAVE_RESULT;
                }
                // 校准超时判断
                if (isovertime) {
                    ui->msgEdit->appendPlainText(QString("校准超时"));
                    state = STATE_OVERTIME_ERROR;
                }
                break;

            case STATE_CHECK_CAIL_RESULT:  // 检查校准结果
                for (int chan = 0; chan < total_sensor; chan++) {
                    if (sensor_v[chan].para.lower_limit <= sensor_v[chan].calib_result &&
                        sensor_v[chan].para.upper_limit >= sensor_v[chan].calib_result) {
                        ui->log->appendPlainText(QString("校准值在阈值范围内:%1-%2")
                                                     .arg(sensor_v[chan].para.lower_limit)
                                                     .arg(sensor_v[chan].para.upper_limit));
                        state = STATE_SEND_CAIL_RESULT;
                    } else {
                        pb->set_press_collect_param(FacSwitch_STOP);
                        delay_msec(200);
                        ui->msgEdit->appendPlainText(QString("校准系数超阈值:%1(%2-%3)")
                                                         .arg(sensor_v[chan].calib_result)
                                                         .arg(sensor_v[chan].para.lower_limit)
                                                         .arg(sensor_v[chan].para.upper_limit));
                        result = failValue;
                        state = STATE_SAVE_RESULT;
                        break;
                    }
                }
                break;

            case STATE_SEND_CAIL_RESULT:  // 开始发送校准值
                if (isCaliOk) {
                    ui->msgEdit->appendPlainText("开始发送校准值");
                    if (product_model == MODEL_ID_U7) {
                        state = STATE_CHECK_NFC;
                    } else {
                        state = STATE_SAVE_CAIL_RESULT;
                    }
                    calib_send_result();  // 发送校准值
                }

                break;

            case STATE_CHECK_NFC:
                if (CheckNfcData() == 6) {
                    ui->msgEdit->appendPlainText("nfc标签校验成功");
                    state = STATE_SAVE_CAIL_RESULT;
                } else {
                    ui->msgEdit->appendPlainText("nfc标签校验失败");
                    result = failValue;
                    state = STATE_SAVE_RESULT;
                }

                if (ui->isusemes->checkState()) {
                    if (stringsn == ui->getMac->text()) {
                        ui->msgEdit->appendPlainText("机内sn与mes比对校验成功");
                    } else {
                        ui->msgEdit->appendPlainText("机内sn与mes比对校验失败");
                        result = failValue;
                        state = STATE_SAVE_RESULT;
                    }
                }
                break;

            case STATE_SAVE_CAIL_RESULT:  // 开始复位操作
                if (repeat_send_ok) {
                    if (pb->get_is_save_press_cali_data())  // 接收校准存入完成
                    {
                        delay_msec(200);
                        if (is_calib_suc == 1) {  // 保存回读成功
                            ui->msgEdit->appendPlainText("已保存完毕");
                            pb->set_dev_reset();  // 开始复位牙刷
                            delay_msec(1000);

                            ui->msgEdit->appendPlainText("已发送复位请求，等待牙刷复位");
                            at->resetConnected();
                            if (function_switch == FUNCTION_CALIB_TEST || function_switch == FUNCTION_TEST) {
                                delay_msec(30);
                                at->sendMac(ui->macInput->text());  // 发送mac地址重连
                                ui->msgEdit->appendPlainText("已发送mac地址");
                                delay_msec(30);
                                state = STATE_WAIT_REST;
                            } else {
                                result = passValue;
                                state = STATE_SAVE_RESULT;
                            }
                        } else {
                            result = failValue;
                            ui->msgEdit->appendPlainText("写入失败，获取结果与写入结果不一致");
                            state = STATE_SAVE_RESULT;
                        }
                    } else {
                        delay_msec(200);
                        pb->get_cali_result();
                        delay_msec(200);
                    }
                }
                break;

                //////////////////////////////////////////////////////////////////////////////////////////////
                ///进入测试流程    ////////////////////////////////////////////////////

            case STATE_WAIT_REST:  // 等待复位操作
                if (at->getConnected()) {
                    ui->msgEdit->appendPlainText("蓝牙已重连");
                    emit refresh_ble_state(1);
                    pb->reset_all_pb();
                    pb->set_forbid_sleep(FacSwitch_OPEN);
                    delay_msec(200);
                    state = STATE_DISABLE_SLEEP_TEST;
                    // state = STATE_SAVE_RESULT;
                } else {
                    emit refresh_ble_state(0);
                }
                break;

            case STATE_DISABLE_SLEEP_TEST:  // 开始第二次禁止休眠
                // delay_msec(100);
                if (pb->getDisableSleep()) {
                    delay_msec(200);
                    ui->msgEdit->appendPlainText("已进入第二次禁止休眠");
                    pb->set_press_collect_param(FacSwitch_START);
                    delay_msec(200);
                    state = STATE_SET_COLLECT_PARAM_2;
                } else {
                    ui->msgEdit->appendPlainText("设置第二次禁止休眠");
                    pb->set_forbid_sleep(FacSwitch_OPEN);
                    delay_msec(200);
                }
                break;

            case STATE_SET_COLLECT_PARAM_2:  // 开始第二次采集参数
                if (pb->getCollectParam()) {
                    delay_msec(200);
                    ui->msgEdit->appendPlainText("已进入第二次设置压感采集参数");
                    state = STATE_TEST_CH_X;
                }
                break;

            case STATE_TEST_PARM_LIMIT:  // 开始第二次采集参数
                if (LastCali.calib_factor[MODULE_BTH] != 0) {
                    for (int chan = 0; chan < total_sensor; chan++) {
                        if (sensor_v[chan].para.lower_limit <= LastCali.calib_factor[chan + 1] &&
                            sensor_v[chan].para.upper_limit >= LastCali.calib_factor[chan + 1]) {
                            ui->log->appendPlainText(QString("校准值在阈值范围内:%1-%2")
                                                         .arg(sensor_v[chan].para.lower_limit)
                                                         .arg(sensor_v[chan].para.upper_limit));
                            state = STATE_TEST_CH_X;
                        } else {
                            pb->set_press_collect_param(FacSwitch_STOP);
                            delay_msec(200);
                            ui->msgEdit->appendPlainText(QString("校准系数超阈值:%1(%2-%3)")
                                                             .arg(LastCali.calib_factor[chan])
                                                             .arg(sensor_v[chan].para.lower_limit)
                                                             .arg(sensor_v[chan].para.upper_limit));
                            result = failValue;
                            state = STATE_SAVE_RESULT;
                            break;
                        }
                    }
                }
                break;

            case STATE_TEST_CH_X:  //测试通道x

                if (sensor_v[test_chan].test_status == TEST_INVALID) {
                    sensor_v[test_chan].sensor_test_status_set(TEST_START);
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                    ui_msg_show(product_model, STATE_TEST_CH_X, test_chan);

                    waittime->setInterval(measure_wait_time);
                    waittime->start();

                    state = STATE_TEST_CH_X_RESULT;
                } else if (sensor_v[test_chan].test_status == TEST_NOTEST) {
                    state = STATE_TEST_CH_X_RESULT;
                }

                break;

            case STATE_TEST_CH_X_RESULT:  //测试通道x结果

                if (sensor_v[test_chan].test_status == TEST_SUC || sensor_v[test_chan].test_status == TEST_NOTEST) {
                    waittime->stop();
                    ui_msg_show(product_model, STATE_TEST_CH_X_RESULT, test_chan);
                    set_fixture_movement(product_model, STATE_TEST_CH_X_RESULT, test_chan);
                    if (test_chan + 1 < total_sensor) {
                        test_chan++;
                        state = STATE_TEST_CH_X;
                        ui->msgEdit->appendPlainText(QString("通道%1开始测试").arg(test_chan));
                    } else {
                        state = STATE_TEST_ALL_RESULT;
                        ui->msgEdit->appendPlainText("所有通道测试完毕");
                    }
                } else if (sensor_v[test_chan].test_status == TEST_FAIL) {
                    waittime->stop();
                    pb->set_press_collect_param(FacSwitch_STOP);
                    delay_msec(200);
                    ui->msgEdit->appendPlainText("压感测试失败");
                    result = failValue;
                    state = STATE_SAVE_RESULT;
                }

                if (isovertime) {
                    ui->msgEdit->appendPlainText(QString("测试超时"));
                    state = STATE_OVERTIME_ERROR;
                }
                break;

            case STATE_TEST_ALL_RESULT:  //测试ALL通道结果
                if (1) {
                    ui_msg_show(product_model, STATE_TEST_ALL_RESULT, test_chan);
                    result = passValue;
                    state = STATE_SAVE_RESULT;
                }
                break;

            case STATE_OVERTIME_ERROR:
                result = failValue;
                state = STATE_SAVE_RESULT;
                break;

            case STATE_SAVE_RESULT:
                // save result
                if (result == passValue) {
                    QString mesresult = "PASS";
                    QString itemvalue;

                    itemvalue = QString("|press_cali:BTH:%1").arg(cali_result.calib_factor[MODULE_BTH]) +
                                QString("|mode_key:%1").arg(cali_result.calib_factor[MODULE_MODE_BUTTON]) +
                                QString("|power_key:%1").arg(cali_result.calib_factor[MODULE_POWER_BUTTON]);

                    pack.result = mesresult;
                    pack.itemvalue = itemvalue;
                    pack.instruct_num = "084";
                    pack.sn = ui->getMac->text();
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }
                    ui->test_result->setText("PASS");
                    ui->test_result->setStyleSheet(
                        "font-size: 68px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 10px; text-align: center;");
                } else if ((result == failValue)) {
                    // mes
                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet(
                        "font-size: 68px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
                        "border-radius: 10px; padding: 10px; text-align: center; ");
                    QString mesresult = "NG";
                    QString itemvalue = QString("|PRESSURE_SENSOR_CALI_RESULT:NG|");
                    pack.result = mesresult;
                    pack.itemvalue = itemvalue;
                    pack.sn = ui->getMac->text();
                    if (ui->isusemes->checkState()) {
                        emit send_end_testPass(pack);
                    }
                }
                set_fixture_movement(product_model, STATE_SAVE_RESULT, 0);
                save_press_test_data_to_csv(macAddress, result, cali_result);
                isTestContinue = false;
                // ui->msgEdit->appendPlainText("压感测试完毕");
                delay_msec(300);
                pb->set_dev_reset();  // 开始复位牙刷
                delay_msec(50);
                ui->msgEdit->appendPlainText("牙刷已复位");
                closeSerialPort();

                stringsn = "";
                ui->macInput->setDisabled(0);
                ui->macInput->clear();

                ui->getMac->clear();

                if (getIndex() == 0) {
                    ui->macInput->setFocus();
                }
                ui->msgEdit->appendPlainText("流程结束");

                state = STATE_IDLE;

                break;

            default: break;
        }

        //  QCoreApplication::processEvents();
    }
    ui->macInput->setEnabled(true);
}

void PressureSensorForm::readData() {
    QByteArray dataTemp;
    dataTemp = dongleSerialPort->readAll();
    // qDebug() << "data len : " << dataTemp.size();
    at->parseCmd(dataTemp);
    pb->parseCmd(dataTemp);
    // ui->log->appendPlainText(QString::fromUtf8(dataTemp));
    // qDebug() << QString::fromUtf8(dataTemp);
}

void PressureSensorForm::on_msg_textChanged() { ui->msgEdit->moveCursor(QTextCursor::End); }

void PressureSensorForm::on_ClearGraph_clicked() { graph_reset(0); }

void PressureSensorForm::on_GetButton_clicked() {
    pb->get_button_state(1);
    return;
}

void PressureSensorForm::over_task() { on_end_clicked(); }

void PressureSensorForm::on_button_get_calib_factor_clicked() {
    delay_msec(200);
    pb->get_cali_result();
    delay_msec(200);
}

uint8_t PressureSensorForm::CheckNfcData() {
    uint8_t ret = 0;
    QString ReadNfcData = ReadNfcDataProcess();

    // 定义各部分的长度
    int lenA = 32;  // A 的长度
    int lenB = 2;   // B 的长度
    int lenC = 24;  // C 的长度
    int lenD = 32;  // D 的长度
    int lenE = 20;  // E 的长度
    int lenF = 12;  // F 的长度
    int lenG = 2;   // G 的长度

    // 使用 mid 方法拆分字符串
    QString model = ReadNfcData.mid(0, lenA);
    QString supid = ReadNfcData.mid(lenA, lenB);
    QString C = ReadNfcData.mid(lenA + lenB, lenC);
    QString sn = ReadNfcData.mid(lenA + lenB + lenC, lenD);
    QString E = ReadNfcData.mid(lenA + lenB + lenC + lenD, lenE);
    QString mac = ReadNfcData.mid(lenA + lenB + lenC + lenD + lenE, lenF);
    QString other = ReadNfcData.mid(lenA + lenB + lenC + lenD + lenE + lenF, lenG);

    // 输出拆分后的结果
    ui->log->appendPlainText("model:" + model);
    ui->log->appendPlainText("supid:" + supid);
    ui->log->appendPlainText("fix_C:" + C);
    ui->log->appendPlainText("sn:" + sn);
    ui->log->appendPlainText("fix_E:" + E);
    ui->log->appendPlainText("mac:" + mac);
    ui->log->appendPlainText("other:" + other);

    QString hexString;
    QString text = stringsn;
    QString truncatedSN = stringsn.left(8);
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    QString value = settings.value("SUBPID/" + truncatedSN, "SUBPID_ERRO").toString();
    ui->log->appendPlainText("匹配到的subpid：" + value);

    for (int i = 0; i < text.length(); ++i) {
        hexString += QString("%1").arg(text[i].toLatin1(), 2, 16, QChar('0')).toUpper();
    }
    ui->log->appendPlainText("转化为:" + hexString);

    // 比对型号
    ui->log->appendPlainText("当前型号为：" + product);
    if (product == "U7P" && model == "033BD2023668772001004800324F3130") {
        ui->log->appendPlainText("Check:U7P");
        ret = 1;
    } else if (product == "U7" && model == "033BD2023668772001004800324F3045") {
        ret = 1;
        ui->log->appendPlainText("Check:U7");
    } else {
        ui->log->appendPlainText("NFC标签:型号编码错误,error model:" + model);
        return ret;
    }

    // 对比supid
    if (value == supid && ret == 1) {
        ui->log->appendPlainText("supid 校验通过");
        ret = 2;
    } else {
        ui->log->appendPlainText("NFC标签:supid错误,error:" + supid);
        ui->log->appendPlainText("correct:" + value);
        return ret;
    }

    if (C == "810800272000141785911410" && ret == 2) {
        ret = 3;
        ui->log->appendPlainText("C 校验通过");
    } else {
        ui->log->appendPlainText("NFC标签:型号编码错误,fix_C:" + C);
        return ret;
    }

    if (sn == hexString && ret == 3) {
        ui->log->appendPlainText("sn 校验通过");
        ret = 4;
    } else {
        ui->log->appendPlainText("NFC标签:sn编码错误,error:" + sn);
        ui->log->appendPlainText("hexString:" + hexString);
        return ret;
    }

    if (E == "170102910B0101010A06" && ret == 4) {
        ui->log->appendPlainText("E 校验通过");
        ret = 5;
    } else {
        ui->log->appendPlainText("NFC标签:fix_e编码错误,error:" + E);
        return ret;
    }

    if (mac == ui->macInput->text().remove(':').right(12) && ret == 5) {
        ui->log->appendPlainText("mac 校验通过");
        ret = 6;
    } else {
        ui->log->appendPlainText("NFC标签:mac编码错误,error:" + mac);
        return ret;
    }

    return ret;
}

int PressureSensorForm::findNfcDevicePort(QString name) {
    int st = -1;
    HANDLE icdev = (HANDLE)-1;
    unsigned char buff_1[8];
    int k = 100;

    for (int i = 0; i < 10; i++) {
        k = k + i;
        icdev = dc_init(k, 115200);
        st = dc_srd_eeprom(icdev, 0, 8, buff_1);
        if (st != 0) {
            qDebug() << "nfc烧录器读取失败";
        } else {
            qDebug() << "nfc烧录器读取成功";
            QString buffStr = QString::fromLatin1(reinterpret_cast<const char*>(buff_1), 8);
            if (buffStr == name) {
                ui->log->appendPlainText("找到设备端口号" + QString::number(k));
                return k;
            }
        }
    }
    ui->log->appendPlainText("没找到设备端口号");
    return 100;
}

QString PressureSensorForm::ReadNfcDataProcess() {
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    QString ReadNfcData = "";
    int dataSize = 62;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    int nfcport = findNfcDevicePort(nfcComName);
    icdev = dc_init(nfcport, 115200);
    if ((intptr_t)icdev <= 0) {
        ui->log->appendPlainText("初始化nfc接口失败!");
        return 0;
    } else {
        ui->log->appendPlainText("初始化nfc接口成功");
    }

    dc_beep(icdev, 10);
    // 射频复位
    dc_reset(icdev, 1);
    st = dc_card_n(icdev, 0, &SnrLen, _Snr);
    if (st != 0) {
        if (st == 1)
            ui->log->appendPlainText("nfc卡识别不到");
        if (st < 0)
            ui->log->appendPlainText("nfc卡查询失败");

        return 0;
    } else {
        ui->log->appendPlainText("nfc卡查询成功");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char*)szSnr;
        ui->log->appendPlainText("卡的序列号为" + QString::fromStdString(str1));
    }

    for (int i = 4; i * 4 < dataSize; i += 4) {  // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0) {
            // ui->msgEdit->appendPlainText("dc_read Error!");
            ui->log->appendPlainText("nfc信息读取失败");
            return 0;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            //   ui->msgEdit->appendPlainText(QString::fromStdString(str1));
        }
    }
    if (dataSize % 16) {
        st = dc_read(icdev, 4 + (dataSize / 16) * 4, rdata);
        if (st != 0) {
            ui->log->appendPlainText("nfc信息读取失败");
            //  ui->msgEdit->appendPlainText("dc_read Error!");
            return 0;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, dataSize % 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            //  ui->msgEdit->appendPlainText(QString::fromStdString(str1));
        }
    }
    ui->log->appendPlainText("nfc信息读取结束");
    ui->log->appendPlainText("nfc内容为：" + ReadNfcData);

    return ReadNfcData;
}

void PressureSensorForm::refreshBaseData(FacGetDevBaseInfo data) {
    product = data.product_name;
    qDebug() << "refreshBaseData";
    ui->log->appendPlainText("获取到当前型号为：" + product);
    ui->log->appendPlainText(data.product_name);
}

void PressureSensorForm::on_TestButtonNFC_clicked() { QString ReadNfcData = ReadNfcDataProcess(); }

void PressureSensorForm::on_TestButton1_clicked() {
    pb->get_base_info();  // 获取设备信息
}

void PressureSensorForm::on_SendCalib_clicked() {
    return;

    press_calib_data_t cali_result;
    cali_result.calib_factor[MODULE_BTH] = 2666;
    cali_result.calib_factor[MODULE_POWER_BUTTON] = 2666;
    cali_result.calib_factor[MODULE_MODE_BUTTON] = 2666;
    cali_result.temperature[MODULE_BTH] = 2666;

    pb->sendCaliResult(cali_result);
}
