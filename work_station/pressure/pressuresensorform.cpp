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
#    pragma execution_character_set(push, "utf-8")
#endif

// y20p
#define ADC_THRESHOLD_Y20P_BTH 350  // 200g:150 400g:350
#define ADC_THRESHOLD_Y20P_KEY 30

#define COUNT_THRESHOLD_Y20P_BTH 300
#define COUNT_THRESHOLD_Y20P_KEY 400

// y20po
#define ADC_THRESHOLD_Y20PO_KEY 40
#define COUNT_THRESHOLD_Y20PO_KEY 400

// f20
#define ADC_THRESHOLD_F20_BTH 60
#define ADC_THRESHOLD_F20_KEY 10

#define COUNT_THRESHOLD_F20_BTH 300
#define COUNT_THRESHOLD_F20_KEY 200

// u7
#define ADC_THRESHOLD_U7_KEY 40
#define COUNT_THRESHOLD_U7_KEY 200

// y25p
#define ADC_THRESHOLD_Y30PS_KEY 40
#define COUNT_THRESHOLD_Y30PS_KEY 200

// y21
#define ADC_THRESHOLD_Y21_BTH 70
#define COUNT_THRESHOLD_Y21_BTH 300

// p30p
#define ADC_THRESHOLD_P30P_KEY 40
#define COUNT_THRESHOLD_P30P_KEY 200

PressureSensorForm::PressureSensorForm(int index, QWidget* parent) : test_base(parent), ui(new Ui::PressureSensorForm) {
    m_index = index;
    pack.mechines = getIndex();

    dongleOutTime = 1;  // 太快会死锁
    upperComputerVer = PRESSURE_VER;

    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
    ui->uartStatusLabel->setText("串口连接：<font color='red'>失败</font>");
    ui->macLabel->setText("蓝牙mac:                        ");
    ui->frame_rate->setText("数据帧率:            ");

    //治具串口不可见
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
    fmt.setFontPointSize(10);
    // 文本框使用以上设定
    ui->log->mergeCurrentCharFormat(fmt);

    scanSerialPorts();
    ui->msgEdit->setStyleSheet("font-size: 10pt;");

    mUserno = SETTINGS.value("Mes/mUserno").toString();
    machineNo = SETTINGS.value("Mes/machineNo").toString();
    ui->msgEdit->appendPlainText("mUserno=" + mUserno);
    ui->msgEdit->appendPlainText("machineNo=" + machineNo);

    actual_wait_time = SETTINGS.value("PRESSURE/TestTime").toInt();
    measure_wait_time = actual_wait_time + 5000;
    pack.product = SETTINGS.value("Mes/Product_Name").toString();
    product_model_init(pack.product);

    graph_init(product_model);

    only_mode = ONLY_SET_E(SETTINGS.value("PRESSURE/Module", 1).toInt());

    function_switch = function_e(SETTINGS.value("PRESSURE/functionSwitch", 1).toInt());
    showlog("当前选择的功能是" + QString::number(function_switch));
    ui->mechine_number->setText(QString::number(getIndex()) + "号机");
    ui->mechine_number->setStyleSheet("font-size: 33px; background-color: yellow; color: black;  border-radius: 10px; "
                                      "padding: 10px; text-align: center; ");
    ui->test_result->setText("WAIT");
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                   "padding: 10px; text-align: center; ");
    ui->mes_state->setText("MES");
    ui->mes_state->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: 10px; "
                                 "padding: 10px; text-align: center; ");

    IsSaveFail = SETTINGS.value("DATA/IsSaveFail").toInt();

    connect(waittime, &QTimer::timeout, [=]() {
        isovertime = 1;
        waittime->stop();
        qDebug() << "定时器结束计时" << QDateTime::currentDateTime();
    });
    ui->tabWidget->setCurrentIndex(0);  // 设置当前页为第一页
    testResultTableInit();

    connect(this, SIGNAL(dataReady()), this, SLOT(updateGraphs()));
}

void PressureSensorForm::graph_init(MODEL_ID_E model) {
    graph_set_argu = GRAPH_SET_E(SETTINGS.value("PRESSURE/Use_graph", 0).toInt());

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
    } else if (model == MODEL_ID_Y30PS) {
        chnal_num = 2;
    } else if (model == MODEL_ID_Y20PO) {
        chnal_num = 2;
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

        y20p_ch0_vector.para.f_module[0] = MODULE_BTH;
        y20p_ch1_vector.para.f_module[0] = MODULE_MODE_BUTTON;

        // y20p_ch1_vector.test_time = 1;
        y20p_ch1_vector.para.test_grams[0] = 200;
        y20p_ch1_vector.para.test_tolerance[0] = 50;
        y20p_ch1_vector.para.test_count[0] = 300;

        y20p_ch0_vector.para.lower_limit = SETTINGS.value("PRESSURE/bth_lower", "1000").toInt();
        y20p_ch0_vector.para.upper_limit = SETTINGS.value("PRESSURE/bth_upper", "3000").toInt();

        y20p_ch1_vector.para.lower_limit = SETTINGS.value("Limit/model_button_lower", "1000").toInt();
        y20p_ch1_vector.para.upper_limit = SETTINGS.value("Limit/model_button_upper", "3000").toInt();

        sensor_v.push_back(y20p_ch0_vector);
        sensor_v.push_back(y20p_ch1_vector);

        sensor_v[0].Sensor_cali_Init(CAL_CHANNEL_Y20_CH0);
        sensor_v[1].Sensor_cali_Init(CAL_CHANNEL_Y20_CH1);

    } else if (model == MODEL_ID_Y30PS) {
        ndt_sensor_cali Y30PS_ch0_vector;
        Y30PS_ch0_vector.ui_msg_tip[0] = donotmove;
        Y30PS_ch0_vector.ui_msg_tip[1] = "按键放400g砝码";
        Y30PS_ch0_vector.ui_msg_tip[2] = "拿走砝码";

        Y30PS_ch0_vector.ui_msg_test[0] = "测试200g不触发";
        Y30PS_ch0_vector.ui_msg_test[1] = "测试400g触发";

        Y30PS_ch0_vector.ui_msg_err[0] = "按键错误码为：";

        Y30PS_ch0_vector.para.adc_threshold = SETTINGS.value("PRESSURE/ButtonThreshold").toInt();

        Y30PS_ch0_vector.para.count_threshold = SETTINGS.value("PRESSURE/ButtonThresholdCount").toInt();

        Y30PS_ch0_vector.para.f_module[0] = MODULE_MODE_BUTTON;
        Y30PS_ch0_vector.para.f_module[1] = MODULE_ASSISTANT_COMPONENT;

        Y30PS_ch0_vector.para.lower_limit = SETTINGS.value("PRESSURE/model_button_lower", "2500").toInt();
        Y30PS_ch0_vector.para.upper_limit = SETTINGS.value("PRESSURE/model_button_upper", "4500").toInt();

        sensor_v.push_back(Y30PS_ch0_vector);

        sensor_v[0].Sensor_cali_Init(CAL_CHANNEL_Y30PS_CH0);

        // ndt_sensor_cali Y25S_ch1_vector;  //假的为了显示图像
        // Y25S_ch1_vector.ui_msg_tip[0] = donotmove;
        // Y25S_ch1_vector.ui_msg_tip[1] = "按键放砝码";
        // Y25S_ch1_vector.ui_msg_tip[2] = "拿走砝码";

        // Y25S_ch1_vector.ui_msg_test[0] = "人员：请放50g砝码";
        // Y25S_ch1_vector.ui_msg_test[1] = "人员：请放350g砝码";
        // Y25S_ch1_vector.ui_msg_test[2] = "人员：请放450g砝码";

        // Y25S_ch1_vector.ui_msg_err[0] = "按键错误码为：";

        // Y25S_ch1_vector.para.adc_threshold = ADC_THRESHOLD_Y25S_KEY;
        // Y25S_ch1_vector.para.count_threshold = COUNT_THRESHOLD_Y25S_KEY;

        // Y25S_ch1_vector.para.f_module[1] = MODULE_ASSISTANT_COMPONENT;

        // Y25S_ch1_vector.para.lower_limit = SETTINGS.value("PRESSURE/model_button_lower", "2500").toInt();
        // Y25S_ch1_vector.para.upper_limit = SETTINGS.value("PRESSURE/model_button_upper", "4500").toInt();

        // sensor_v.push_back(Y25S_ch1_vector);

        // sensor_v[0].Sensor_cali_Init(CAL_CHANNEL_Y25S_CH1);

    } else if (model == MODEL_ID_Y20PO) {
        ndt_sensor_cali Y20PO_ch0_vector;
        Y20PO_ch0_vector.ui_msg_tip[0] = donotmove;
        Y20PO_ch0_vector.ui_msg_tip[1] = "按键放砝码";
        Y20PO_ch0_vector.ui_msg_tip[2] = "拿走砝码";

        Y20PO_ch0_vector.ui_msg_test[0] = "测试100g不触发";
        Y20PO_ch0_vector.ui_msg_test[1] = "测试300g触发";

        Y20PO_ch0_vector.ui_msg_err[0] = "按键错误码为：";

        Y20PO_ch0_vector.para.adc_threshold = SETTINGS.value("PRESSURE/ButtonThreshold").toInt();

        Y20PO_ch0_vector.para.count_threshold = SETTINGS.value("PRESSURE/ButtonThresholdCount").toInt();

        Y20PO_ch0_vector.para.f_module[0] = MODULE_MODE_BUTTON;
        Y20PO_ch0_vector.para.f_module[1] = MODULE_ASSISTANT_COMPONENT;
        Y20PO_ch0_vector.para.lower_limit = SETTINGS.value("PRESSURE/model_button_lower", "2500").toInt();
        Y20PO_ch0_vector.para.upper_limit = SETTINGS.value("PRESSURE/model_button_upper", "4500").toInt();

        sensor_v.push_back(Y20PO_ch0_vector);

        sensor_v[0].Sensor_cali_Init(CAL_CHANNEL_Y20PO_CH0);

        // ndt_sensor_cali Y20PO_ch1_vector;  //假的
        // Y20PO_ch1_vector.ui_msg_tip[0] = donotmove;
        // Y20PO_ch1_vector.ui_msg_tip[1] = "按键放砝码";
        // Y20PO_ch1_vector.ui_msg_tip[2] = "拿走砝码";

        // Y20PO_ch1_vector.ui_msg_test[0] = "人员：请放50g砝码";
        // Y20PO_ch1_vector.ui_msg_test[1] = "人员：请放350g砝码";
        // Y20PO_ch1_vector.ui_msg_test[2] = "人员：请放450g砝码";

        // Y20PO_ch1_vector.ui_msg_err[0] = "按键错误码为：";

        // Y20PO_ch1_vector.para.adc_threshold = ADC_THRESHOLD_Y20PO_KEY;
        // Y20PO_ch1_vector.para.count_threshold = COUNT_THRESHOLD_Y20PO_KEY;

        // Y20PO_ch1_vector.para.f_module = MODULE_ASSISTANT_COMPONENT;

        // Y20PO_ch1_vector.para.lower_limit = SETTINGS.value("PRESSURE/model_button_lower", "2500").toInt();
        // Y20PO_ch1_vector.para.upper_limit = SETTINGS.value("PRESSURE/model_button_upper", "4500").toInt();

        // sensor_v.push_back(Y20PO_ch1_vector);

        // sensor_v[0].Sensor_cali_Init(CAL_CHANNEL_Y20PO_CH1);

    }

    else if (model == MODEL_ID_U7) {
        ndt_sensor_cali u7_ch0_vector;
        u7_ch0_vector.ui_msg_tip[0] = donotmove;
        u7_ch0_vector.ui_msg_tip[1] = "按键放300g砝码";
        u7_ch0_vector.ui_msg_tip[2] = "拿走砝码";

        u7_ch0_vector.ui_msg_test[0] = "测试100g不触发";
        u7_ch0_vector.ui_msg_test[1] = "测试300g触发";

        u7_ch0_vector.ui_msg_err[0] = "按键错误码为：";

        u7_ch0_vector.para.adc_threshold = ADC_THRESHOLD_U7_KEY;
        u7_ch0_vector.para.count_threshold = COUNT_THRESHOLD_U7_KEY;

        u7_ch0_vector.para.f_module[0] = MODULE_MODE_BUTTON;

        u7_ch0_vector.para.lower_limit = SETTINGS.value("PRESSURE/model_button_lower", "2500").toInt();
        u7_ch0_vector.para.upper_limit = SETTINGS.value("PRESSURE/model_button_upper", "4500").toInt();

        sensor_v.push_back(u7_ch0_vector);

        sensor_v[0].Sensor_cali_Init(CAL_CHANNEL_U7_CH0);

    } else if (model == MODEL_ID_P30P) {
        ndt_sensor_cali p30p_ch0_vector;
        p30p_ch0_vector.ui_msg_tip[0] = donotmove;
        p30p_ch0_vector.ui_msg_tip[1] = "按键放300g砝码";
        p30p_ch0_vector.ui_msg_tip[2] = "拿走砝码";

        p30p_ch0_vector.ui_msg_err[0] = "刷头错误码为：";

        p30p_ch0_vector.ui_msg_test[0] = "测试100g不触发";
        p30p_ch0_vector.ui_msg_test[1] = "测试300g触发";

        p30p_ch0_vector.para.adc_threshold = ADC_THRESHOLD_P30P_KEY;
        p30p_ch0_vector.para.count_threshold = COUNT_THRESHOLD_P30P_KEY;

        p30p_ch0_vector.para.f_module[0] = MODULE_POWER_BUTTON;

        p30p_ch0_vector.para.lower_limit = SETTINGS.value("PRESSURE/power_button_lower", "500").toInt();
        p30p_ch0_vector.para.upper_limit = SETTINGS.value("PRESSURE/power_button_upper", "4500").toInt();

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
    showlog(str);
}

void PressureSensorForm::getTestValue(const int mechines, const QString value) {
    showlog(value);
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
    if (x.brush_head_adc != 0) {
        showlog("读取到的刷头校准系数" + QString::number(x.brush_head_adc));
        LastCali.calib_factor[MODULE_BTH] = x.brush_head_adc;
        qDebug() << "获取到刷头校准系数：" << LastCali.calib_factor[MODULE_BTH];
    }
    if (x.mode_button_adc != 0) {
        showlog("读取到的模式按键校准系数" + QString::number(x.mode_button_adc));
        LastCali.calib_factor[MODULE_MODE_BUTTON] = x.mode_button_adc;
        qDebug() << "获取到模式按键校准系数：" << LastCali.calib_factor[MODULE_MODE_BUTTON];
    }
    if (x.power_button_adc != 0) {
        showlog("读取到的电源按键校准系数" + QString::number(x.power_button_adc));
        LastCali.calib_factor[MODULE_POWER_BUTTON] = x.power_button_adc;
        qDebug() << "获取到电源按键校准系数：" << LastCali.calib_factor[MODULE_MODE_BUTTON];
    }
    if (x.temperature != 0) {
        showlog("读取到的温度校准系数" + QString::number(x.temperature));
        LastCali.temperature[MODULE_BTH] = x.temperature;
        qDebug() << "获取到温度系数：" << LastCali.calib_factor[MODULE_MODE_BUTTON];
    }

    if (x.assistant_component != 0) {
        showlog("读取到的辅助元器件校准系数" + QString::number(x.assistant_component));
        LastCali.calib_factor[MODULE_ASSISTANT_COMPONENT] = x.assistant_component;
        qDebug() << "辅助元器件" << LastCali.calib_factor[MODULE_ASSISTANT_COMPONENT];
    }

    // if (x.assistant_component == (uint32_t)cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT] &&
    //     x.brush_head_adc == (uint32_t)cali_result.calib_factor[MODULE_BTH] &&
    //     x.mode_button_adc == (uint32_t)cali_result.calib_factor[MODULE_MODE_BUTTON] &&
    //     x.power_button_adc == (uint32_t)cali_result.calib_factor[MODULE_POWER_BUTTON] &&
    //     (x.temperature == (uint32_t)cali_result.temperature[MODULE_BTH] ||
    //      x.temperature == (uint32_t)cali_result.temperature[MODULE_MODE_BUTTON])) {
    //     qDebug() << "校准数据核对成功";
    //     is_calib_suc = 1;
    // }

    //校准值是0就不比对
    if ((cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT] == 0 ||
         x.assistant_component == (uint32_t)cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT]) &&
        (cali_result.calib_factor[MODULE_BTH] == 0 ||
         x.brush_head_adc == (uint32_t)cali_result.calib_factor[MODULE_BTH]) &&
        (cali_result.calib_factor[MODULE_MODE_BUTTON] == 0 ||
         x.mode_button_adc == (uint32_t)cali_result.calib_factor[MODULE_MODE_BUTTON]) &&
        (cali_result.calib_factor[MODULE_POWER_BUTTON] == 0 ||
         x.power_button_adc == (uint32_t)cali_result.calib_factor[MODULE_POWER_BUTTON]) &&
        (x.temperature == (uint32_t)cali_result.temperature[MODULE_BTH] ||
         x.temperature == (uint32_t)cali_result.temperature[MODULE_MODE_BUTTON])) {
        qDebug() << "校准数据核对成功";
        is_calib_suc = 1;
    }
}

PressureSensorForm::~PressureSensorForm() { delete ui; }

void PressureSensorForm::refreshJigUartState(int state) {
    if (state) {
        showlog("治具串口连接成功");
        ui->Jiguartstate->setText("治具串口连接：<font color='green'>成功</font>");
    } else {
        ui->jigComNameCombo->setEnabled(true);
        ui->jigConnectButton->setEnabled(true);
        showlog("治具串口连接断开");
        ui->Jiguartstate->setText("治具串口连接：<font color='red'>失败</font>");
    }
}

void PressureSensorForm::on_jigConnectButton_clicked() {
    openJigSerialPort();
    ui->jigComNameCombo->setEnabled(false);
    ui->jigConnectButton->setEnabled(false);
}

void PressureSensorForm::on_jigDisconnectButton_clicked() {
    closeJigSerialPort();
    ui->jigComNameCombo->setEnabled(true);
    ui->jigConnectButton->setEnabled(true);
}

void PressureSensorForm::receive_uart_data(int state) {
    qDebug() << "receive_uart_data:" << state;
    qDebug() << "getIndex():" << getIndex();
    if (state == 1) {
        getaskbarcode = 1;
    } else if (state == 2) {
        is_get_ready_command = true;
        set_independent_state(STATE_CALIB_START);
    } else if (state == 3) {
        if (getIndex() == 1) {
            set_independent_state(STATE_CALIB_START);
        }
    } else if (state == 4) {
        if (getIndex() == 2) {
            set_independent_state(STATE_CALIB_START);
        }
    }
}

void PressureSensorForm::readJigSerialPortData() {
    QByteArray dataTemp;
    dataTemp = jigSerialPort->readAll();
    // qusb->parseCmd(dataTemp);
    qDebug() << QString::fromUtf8(dataTemp);
    // 处理接收到的数据
    processFixReceivedData(dataTemp);
}

void PressureSensorForm::processFixReceivedData(const QByteArray& data) {
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

void PressureSensorForm::send_frame_rate(QString data) { ui->frame_rate->setText("数据帧率: " + data); }

void PressureSensorForm::refreshDongleUartState(int state) {
    if (state) {
        showlog("dongle串口连接成功");
        ui->uartStatusLabel->setText("串口连接：<font color='green'>成功</font>");
    } else {
        ui->uartStatusLabel->setText("串口连接：<font color='red'>失败</font>");
        ui->comNameCombo->setEnabled(true);
        ui->connectButton->setEnabled(true);
        showlog("dongle串口连接断开");
    }
}

void PressureSensorForm::send_start_command(machine_command_id_e command_id, int argument) {
    FixturePacketData packet_data;

    packet_data.machine_command_id = command_id;
    packet_data.argument = argument;
    packet_data.machineNumber = getIndex() - 1;

    qDebug() << "command_id:" << command_id << "argument:" << argument << "  packet_data.machineNumber"
             << packet_data.machineNumber;

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

void PressureSensorForm::on_disconnectButton_clicked() {
    ui->comNameCombo->setEnabled(true);
    ui->connectButton->setEnabled(true);
    closeDongleSerialPort();
}

void PressureSensorForm::on_connectButton_clicked() {
    ui->comNameCombo->setEnabled(false);
    ui->connectButton->setEnabled(false);
    openDongleSerialPort();
}

void PressureSensorForm::save_press_test_data_to_csv(const QString& macAddress, press_calib_data_t cali_result) {
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
        rowData << QString::number(getIndex());
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
            for (int i = 0; i < total_sensor; i++) {  //双通道共用一个校准结果
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

void PressureSensorForm::clear_display() {
    showlog("clear_display");
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
        QMessageBox::warning(nullptr, "Warning", QString("%1号机的Mac地址错误").arg(getIndex()));
        return;
    } else {
        macAddress = ui->macInput->text();

        macAddress = ui->macInput->text();
        ui->macLabel->setText("蓝牙mac: " + macAddress);

        ui->macInput->setEnabled(false);
        stringsn = "";
        state = STATE_IDLE;
        isTestContinue = true;
        // emit gonextfocus();
        emit send_go_next_focus();

        ui->test_result->setText("WAIT");
        ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  border-radius: "
                                       "10px; padding: 10px; text-align: center; ");
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
    ui->test_result->setStyleSheet("font-size: 33px; background-color: #808080; color: black;  "
                                   "border-radius: 10px; padding: 10px; text-align: center; ");

    // 检查是否是序列号格式
    QRegularExpression snRegex(snPattern);
    // 使用正则表达式匹配
    if (!snRegex.match(ui->getMac->text()).hasMatch()) {
        ui->getMac->setDisabled(0);
        ui->macInput->setDisabled(0);

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
    ui->macInput->clear();
    ui->getMac->clear();

    ui->macInput->setDisabled(0);
    ui->getMac->setDisabled(0);
    qDebug() << "on_end_clicked";
    isTestContinue = false;
    if (at->getConnected()) {
        delay_msec(300);
        pb->set_dev_reset();  // 开始复位牙刷
        delay_msec(50);
        ui->msgEdit->appendPlainText("牙刷已复位");
    }

    if (dongleSerialPort->isOpen()) {
        closeDongleSerialPort();
    }

    // set_independent_state(STATE_CALIB_RESULT);
    // set_fixture_movement(product_model, STATE_SAVE_RESULT, 0);
    ui->getMac->setFocus();
}

void PressureSensorForm::savePressDataToLocalFolder(const FacUploadPresSensor& x, bool appHeader) {
    QString folderPath = "D:/测试结果";
    // 如果 "测试结果" 文件夹不存在，则创建它
    if (!QDir(folderPath).exists()) {
        QDir().mkpath(folderPath);
    }
    // 获取当前日期
    QDate currentDate = QDate::currentDate();
    // 构建完整的文件路径，加上日期
    QString fileName = currentDate.toString("yyyy-MM-dd") + "_压感数据报告.csv";
    QString filePath = QDir(folderPath).filePath(fileName);
    // 打开文件，如果无法打开则返回
    QFile file(filePath);
    if (!file.open(QIODevice::Append | QIODevice::Text)) {
        qWarning("无法打开文件");
        return;
    }
    // 创建文本流对象
    QTextStream out(&file);
    if (file.pos() == 0 || appHeader) {
        QStringList headers;
        headers << "时间戳"
                << "MAC地址"
                << "刷头ADC"
                << "刷头压力"
                << "模式按键ADC"
                << "模式按键压力"
                << "电源按键ADC"
                << "电源按键压力"
                << "辅助元件ADC"
                << "辅助元件压力";
        out << headers.join(",") << "\n";
    }
    // 获取当前时间戳
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString timestamp = currentDateTime.toString("yyyy-MM-dd hh:mm:ss");

    // 遍历传感器数据并写入文件
    for (int i = 0; i < x.sensor_data_count; i++) {
        QStringList dataList;
        dataList << timestamp << macAddress << QString::number(static_cast<int16_t>(x.sensor_data[i].brush_head.adc))
                 << QString::number(static_cast<int16_t>(x.sensor_data[i].brush_head.value))
                 << QString::number(static_cast<int16_t>(x.sensor_data[i].mode_button.adc))
                 << QString::number(static_cast<int16_t>(x.sensor_data[i].mode_button.value))
                 << QString::number(static_cast<int16_t>(x.sensor_data[i].power_button.adc))
                 << QString::number(static_cast<int16_t>(x.sensor_data[i].power_button.value))
                 << QString::number(static_cast<int16_t>(x.sensor_data[i].assistant_component.adc))
                 << QString::number(static_cast<int16_t>(x.sensor_data[i].assistant_component.value));

        out << dataList.join(",") << "\n";
    }
    // 关闭文件
    file.close();
}
void PressureSensorForm::product_model_init(QString model) {
    showlog("product_model_init is " + model);
    if (model == NULL) {
        qDebug() << "error,model == NULL";
        return;
    }

    if (model == "Y20P") {
        product_model = MODEL_ID_Y20P;
    } else if (model == "F20") {
        product_model = MODEL_ID_F20;
    } else if (model == "U7" || model == "U7P") {
        product_model = MODEL_ID_U7;
    } else if (model == "Y21") {
        product_model = MODEL_ID_Y21;
    } else if (model == "P30P") {
        product_model = MODEL_ID_P30P;
    } else if (model == "Y30PS") {
        product_model = MODEL_ID_Y30PS;
    } else if (model == "Y20PO") {
        product_model = MODEL_ID_Y20PO;
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

void PressureSensorForm::updateGraphs() {
    int need_sensor_data = total_sensor;
    if (product_model == MODEL_ID_Y30PS || product_model == MODEL_ID_Y20PO)
        need_sensor_data = 2;
    else
        need_sensor_data = 1;
    // 从队列中取出数据并更新图表
    while (!adcDataQueue.isEmpty()) {
        auto adcData = adcDataQueue.dequeue();
        auto valueData = valueDataQueue.dequeue();

        uint32_t timestamp = adcData.first;
        const QVector<int16_t>& adcValues = adcData.second;
        const QVector<int16_t>& valueValues = valueData.second;

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; need_sensor_count++) {
                int index = channel + need_sensor_count;
                if (index >= adcValues.size() || index >= valueValues.size())
                    continue;

                int adc = adcValues[index];
                int value = valueValues[index];

                graph_adc_vector[channel + need_sensor_count]->graph(0)->addData(timestamp, adc);
                graph_value_vector[channel + need_sensor_count]->graph(0)->addData(timestamp, value);

                // 更新图表范围
                uint32_t x_min = 0;
                if (counter >= 20000) {
                    x_min = counter - 20000;
                }
                graph_adc_vector[channel + need_sensor_count]->xAxis->setRange(x_min, counter);
                graph_adc_vector[channel + need_sensor_count]->graph(0)->rescaleValueAxis();
                graph_adc_vector[channel + need_sensor_count]->replot();

                graph_value_vector[channel + need_sensor_count]->xAxis->setRange(x_min, counter);
                graph_value_vector[channel + need_sensor_count]->graph(0)->rescaleValueAxis();
                graph_value_vector[channel + need_sensor_count]->replot();

                // 更新 UI 文本
                switch (sensor_v[channel].para.f_module[need_sensor_count]) {
                    case MODULE_BTH:
                        ui->brush_adc->setText("刷头adc：" + QString::number(adc));
                        ui->brush_value->setText("刷头压力：" + QString::number(value));
                        graph_adc_vector[channel + need_sensor_count]->graph(0)->setName("刷头ADC值");
                        graph_value_vector[channel + need_sensor_count]->graph(0)->setName("刷头压力值");
                        break;
                    case MODULE_MODE_BUTTON:
                        ui->botton_adc->setText("模式按键adc：" + QString::number(adc));
                        ui->botton_value1->setText("模式按键压力：" + QString::number(value));
                        graph_adc_vector[channel + need_sensor_count]->graph(0)->setName("模式按键ADC值");
                        graph_value_vector[channel + need_sensor_count]->graph(0)->setName("模式按键压力值");
                        break;
                    case MODULE_POWER_BUTTON:
                        ui->power_adc->setText("电源按键adc：" + QString::number(adc));
                        ui->power_value->setText("电源按键压力：" + QString::number(value));
                        graph_adc_vector[channel + need_sensor_count]->graph(0)->setName("电源按键ADC值");
                        graph_value_vector[channel + need_sensor_count]->graph(0)->setName("电源按键压力值");
                        break;
                    case MODULE_ASSISTANT_COMPONENT:
                        ui->assistant_botton_adc->setText("辅助元件adc：" + QString::number(adc));
                        ui->assistant_botton_value->setText("辅助元件压力：" + QString::number(value));
                        graph_adc_vector[channel + need_sensor_count]->graph(0)->setName("辅助元件ADC值");
                        graph_value_vector[channel + need_sensor_count]->graph(0)->setName("辅助元件压力值");
                        break;
                }
            }
        }

        // 处理帧率显示和倒计时
        send_frame_rate(QString("%1 ms").arg(transTime.elapsed() / adcValues.size()));
        transTime.restart();
        ui->countdown->display((actual_wait_time - countdowntime.elapsed()) / 1000);
    }
}
void PressureSensorForm::graph_update(FacUploadPresSensor x) {
    if (graph_set_argu <= GRAPH_SET_CLOSE) {
        return;
    }

    int need_sensor_data = total_sensor;
    if (product_model == MODEL_ID_Y30PS || product_model == MODEL_ID_Y20PO)
        need_sensor_data = 2;
    else
        need_sensor_data = 1;

    for (int i = 0; i < x.sensor_data_count; i++) {
        QVector<int16_t> adcValues;
        QVector<int16_t> valueValues;

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; need_sensor_count++) {
                switch (sensor_v[channel].para.f_module[need_sensor_count]) {
                    case MODULE_BTH:
                        adcValues.append(x.sensor_data[i].brush_head.adc);
                        valueValues.append(x.sensor_data[i].brush_head.value);
                        break;
                    case MODULE_MODE_BUTTON:
                        adcValues.append(x.sensor_data[i].mode_button.adc);
                        valueValues.append(x.sensor_data[i].mode_button.value);
                        break;
                    case MODULE_POWER_BUTTON:
                        adcValues.append(x.sensor_data[i].power_button.adc);
                        valueValues.append(x.sensor_data[i].power_button.value);
                        break;
                    case MODULE_ASSISTANT_COMPONENT:
                        adcValues.append(x.sensor_data[i].assistant_component.adc);
                        valueValues.append(x.sensor_data[i].assistant_component.value);
                        break;
                    case MODULE_INVALID:
                    case MODULE_MAX: break;
                }
            }
        }

        // 使用 counter 作为时间戳，将数据构造成 QPair 后入队
        adcDataQueue.enqueue(qMakePair(counter, adcValues));
        valueDataQueue.enqueue(qMakePair(counter, valueValues));
        // 发射信号
        emit dataReady();

        counter += 10;
    }
}

void PressureSensorForm::save_pressure_data(FacUploadPresSensor x) {
    for (int i = 0; i < x.sensor_data_count; i++) {
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

            savePressDataToLocalFolder(x, isfirstsavedata);

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
            switch (sensor_v[channel].para.f_module[0]) {
                case MODULE_BTH:
                    cali_result.calib_factor[MODULE_BTH] = sensor_v[channel].calib_result[0];
                    cali_result.temperature[MODULE_BTH] = sensor_v[channel].temperature;
                    ui->msgEdit->appendPlainText(QString("写入刷头校准系数：") +
                                                 QString::number(cali_result.calib_factor[MODULE_BTH]));
                    ui->msgEdit->appendPlainText(QString("写入温度校准系数：") +
                                                 QString::number(cali_result.temperature[MODULE_BTH]));
                    qDebug() << "刷头校准系数：" << cali_result.calib_factor[MODULE_BTH];
                    break;
                case MODULE_MODE_BUTTON:
                    cali_result.calib_factor[MODULE_MODE_BUTTON] = sensor_v[channel].calib_result[0];
                    cali_result.temperature[MODULE_MODE_BUTTON] = sensor_v[channel].temperature;
                    ui->msgEdit->appendPlainText(QString("写入模式按键校准系数：") +
                                                 QString::number(cali_result.calib_factor[MODULE_MODE_BUTTON]));
                    if (sensor_v[channel].temperature != 0) {
                        ui->msgEdit->appendPlainText(QString("写入温度校准系数：") +
                                                     QString::number(cali_result.temperature[MODULE_MODE_BUTTON]));
                    }
                    qDebug() << "模式按键校准系数：" << cali_result.calib_factor[MODULE_MODE_BUTTON];

                    if (product_model == MODEL_ID_Y30PS || product_model == MODEL_ID_Y20PO) {
                        cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT] = sensor_v[channel].calib_result[1];
                        showlog(QString("写入辅助器件校准系数：") +
                                QString::number(cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT]));
                    }

                    break;
                case MODULE_POWER_BUTTON:
                    cali_result.calib_factor[MODULE_POWER_BUTTON] = sensor_v[channel].calib_result[0];
                    ui->msgEdit->appendPlainText(QString("写入电源按键校准系数：") +
                                                 QString::number(cali_result.calib_factor[MODULE_POWER_BUTTON]));
                    qDebug() << "电源按键校准系数：" << cali_result.calib_factor[MODULE_POWER_BUTTON];
                    break;
                case MODULE_ASSISTANT_COMPONENT:
                    cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT] = sensor_v[channel].calib_result[0];
                    showlog(QString("写入辅助器件校准系数：") +
                            QString::number(cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT]));

                    break;
            }
        }

        isStartSendCaliResult = 0;
        delay_msec(300);
        pb->set_press_cali_result(cali_result);
        repeat_send_ok = 1;
        delay_msec(300);
    }
}

// 模组ADC不在范围内（超出-27000 ~ 27000）（3V±250mv）
// 1. 传感器模组损坏
// 2. 传感器接触不良（如：FPC排线断触、断裂）
// 2 未加负载时，ADC抖动太大，噪声计算过大（计算标准差值超出范围）
// 1. 提示不能加负载时，加了负载
// 2. 提示不能加负载时加了负载，治具的动作过大，抖动传导到传感器上。
// 3. 传感器模组损坏，自身ADC值飘动

// 4 噪声过大（噪声峰值超出范围）

// 6 错误码2和4的结合

// 8 挂上负载后信号量太小

// 1. 传感器模组损坏
// 2. 挂的负载不对
// 3. 传感器与受力组件贴合得有问题，或力传导出现问题（例如：脱胶、与传感器本应过盈配合的组件未过盈配合）

//一种类型是2个元器件，校准实例化两个，一种是2个元器件，校准示例化一次就好。
void PressureSensorForm::calib_process(FacUploadPresSensor x) {
    if (total_sensor <= 0) {
        qDebug("错误的传感器数量:%d", total_sensor);
        return;
    }

    for (int i = 0; i < x.sensor_data_count; i++) {
        int first_runing_suc_num = 0;

        int need_sensor_data = total_sensor;
        if (product_model == MODEL_ID_Y30PS || product_model == MODEL_ID_Y20PO)
            need_sensor_data = 2;
        else
            need_sensor_data = 1;

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; need_sensor_count++) {
                switch (sensor_v[channel].para.f_module[need_sensor_count]) {
                    case MODULE_BTH: adc_c[channel + need_sensor_count] = x.sensor_data[i].brush_head.adc; break;
                    case MODULE_MODE_BUTTON:
                        adc_c[channel + need_sensor_count] = x.sensor_data[i].mode_button.adc;
                        break;
                    case MODULE_POWER_BUTTON:
                        adc_c[channel + need_sensor_count] = x.sensor_data[i].power_button.adc;
                        break;
                    case MODULE_ASSISTANT_COMPONENT:
                        adc_c[channel + need_sensor_count] = x.sensor_data[i].assistant_component.adc;
                        break;
                }
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
            if (first_runing_suc_num == total_sensor) {  //要所有通道都OK
                first_start_test = 0;
                for (int i = 0; i < total_sensor; i++) {
                    sensor_v[i].para.first_adc = adc_c[i];
                    showlog("记录初始adc值" + QString::number(sensor_v[i].para.first_adc));
                }
            }
            continue;
        }
        // 避免刷头给按键校准造成干扰需要检查 brush_head.value < 50，即刷头值较小时才进行校准，避免干扰
        if (sensor_v[calib_chan].para.f_module[0] == MODULE_MODE_BUTTON) {
            if (x.sensor_data[i].brush_head.value < 50) {
                // qDebug() << "ADC 差值: " << adc_c[calib_chan] - sensor_v[calib_chan].para.first_adc
                //          << " ADC 阈值: " << sensor_v[calib_chan].para.adc_threshold
                //          << " 校准是否已开始: " << start_calib_channel[calib_chan] << " 校准通道: " << calib_chan
                //          << " 初始 ADC 值: " << sensor_v[calib_chan].para.first_adc;

                if ((adc_c[calib_chan] - sensor_v[calib_chan].para.first_adc) >
                        sensor_v[calib_chan].para.adc_threshold &&
                    !start_calib_channel[calib_chan] && sensor_v[calib_chan].para.first_adc != 0) {
                    brush_before_calib_count++;
                    qDebug() << getIndex() << "合法值个数" << brush_before_calib_count << "要求个数"
                             << sensor_v[calib_chan].para.count_threshold;
                    if (brush_before_calib_count > sensor_v[calib_chan].para.count_threshold) {
                        start_calib_channel[calib_chan] = 1;
                        brush_before_calib_count = 0;
                    }
                }
            }
        } else {
            // qDebug() << "ADC 差值: " << adc_c[calib_chan] - sensor_v[calib_chan].para.first_adc
            //          << " ADC 阈值: " << sensor_v[calib_chan].para.adc_threshold
            //          << " 校准是否已开始: " << start_calib_channel[calib_chan] << " 校准通道: " << calib_chan
            //          << " 初始 ADC 值: " << sensor_v[calib_chan].para.first_adc;

            if ((adc_c[calib_chan] - sensor_v[calib_chan].para.first_adc) > sensor_v[calib_chan].para.adc_threshold &&
                !start_calib_channel[calib_chan] && sensor_v[calib_chan].para.first_adc != 0) {
                brush_before_calib_count++;
                qDebug() << getIndex() << "合法值个数" << brush_before_calib_count << "要求个数"
                         << sensor_v[calib_chan].para.count_threshold;
                if (brush_before_calib_count > sensor_v[calib_chan].para.count_threshold) {
                    start_calib_channel[calib_chan] = 1;
                    brush_before_calib_count = 0;
                }
            }
        }

        if (sensor_v[calib_chan].gs32SensorFlag == 1) {
            // sensor_v[calib_chan].ndt_sensor_cali_process(x.sensor_data_count, adc_c + calib_chan);
        }

        // qDebug() << "Condition met: gs32SensorFlag == 2" << sensor_v[calib_chan].gs32SensorFlag
        //          << "start_calib_channel == " << start_calib_channel[calib_chan]
        //          << ", calib_status == " << sensor_v[calib_chan].calib_status;
        if (sensor_v[calib_chan].gs32SensorFlag == 2 && start_calib_channel[calib_chan] &&
            sensor_v[calib_chan].calib_status != CALIB_NOCALIB) {
            sensor_v[calib_chan].ndt_sensor_cali_process(x.sensor_data_count, adc_c + calib_chan);
        }

        if (sensor_v[calib_chan].gs32SensorFlag == 3) {
            unsigned short* cali_ok =
                sensor_v[calib_chan].ndt_sensor_cali_process(x.sensor_data_count, adc_c + calib_chan);

            sensor_v[calib_chan].calib_status = CALIB_SUC;
            // qDebug() << "cali_ok[0] " << cali_ok[0];

            // qDebug() << "calib_chan " << calib_chan;
            // qDebug() << " sensor_v[calib_chan]calib_status" << sensor_v[calib_chan].calib_status;
            //  qDebug() << " sensor_v[calib_chan].calib_result[1]" << sensor_v[calib_chan].calib_result[1];
            // qDebug() << " sensor_v[calib_chan].calib_result[0]" << sensor_v[calib_chan].calib_result[0];

            if (cali_ok[0] != 0) {
                sensor_v[calib_chan].calib_result = cali_ok;

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

            if (product_model == MODEL_ID_Y30PS || product_model == MODEL_ID_Y20PO) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_err[0] +
                                             QString::number(sensor_v[calib_chan].err[0]));
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_err[0] +
                                             QString::number(sensor_v[calib_chan].err[1]));
            } else {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_err[0] +
                                             QString::number(sensor_v[calib_chan].err[0]));
            }
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
    int need_sensor_data = total_sensor;
    if (product_model == MODEL_ID_Y30PS || product_model == MODEL_ID_Y20PO)
        need_sensor_data = 2;
    else
        need_sensor_data = 1;

    for (int i = 0; i < x.sensor_data_count; i++) {
        // int first_runing_suc_num = 0;
        // int calib_suc_num = 0;
        // int adc = int16_t(x.sensor_data[i].brush_head.adc);
        // int value = int16_t(x.sensor_data[i].brush_head.value);
        // int botton_adc = int16_t(x.sensor_data[i].mode_button.adc);
        // int botton_value = int16_t(x.sensor_data[i].mode_button.value);

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; need_sensor_count++) {
                switch (sensor_v[channel].para.f_module[need_sensor_count]) {
                    case MODULE_BTH:
                        adc_c[channel + need_sensor_count] = x.sensor_data[i].brush_head.adc;
                        value_c[channel + need_sensor_count] = x.sensor_data[i].brush_head.value;
                        break;
                    case MODULE_MODE_BUTTON:
                        adc_c[channel + need_sensor_count] = x.sensor_data[i].mode_button.adc;
                        value_c[channel + need_sensor_count] = x.sensor_data[i].mode_button.value;
                        break;
                    case MODULE_POWER_BUTTON:
                        adc_c[channel + need_sensor_count] = x.sensor_data[i].power_button.adc;
                        value_c[channel + need_sensor_count] = x.sensor_data[i].power_button.value;
                        break;
                }
            }
        }

        switch (sensor_v[test_chan].para.f_module[0]) {
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

#if 0  // 打开就无视电容触发
            if (sensor_v[test_chan].para.no_click_max < value_c[test_chan]) {
                sensor_v[test_chan].para.no_click_max = value_c[test_chan];
            }
            if (sensor_v[test_chan].para.no_click_max > 200) {
                sensor_v[test_chan].para.button_state = 1;
            }
#endif
            if (sensor_v[test_chan].send_state == 1) {
                sensor_v[test_chan].send_state = 0;  //复位治具状态
                if (sensor_v[test_chan].para.button_state == 0) {
                    // sensor_v[test_chan].test_result[0] = 100;
                    sensor_v[test_chan].para.button_state = 0;
                    sensor_v[test_chan].test_status = TEST_KET_CLICK;
                    ui->msgEdit->appendPlainText(sensor_v[test_chan].ui_msg_test[1]);
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                } else if (sensor_v[test_chan].para.button_state == 1) {
                    sensor_v[test_chan].test_status = TEST_FAIL;
                    ui->msgEdit->appendPlainText("测试失败：不需要触发，按键触发");
                }
            }
        } else if (sensor_v[test_chan].test_status == TEST_KET_CLICK) {
            qDebug("TEST_KET_CLICK:%d", sensor_v[test_chan].para.current_count);
// 无视电容触发
#if 0
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
                    ui->msgEdit->appendPlainText("测试失败：需要触发，按键未触发");
                }
            }
        }
    }
}

void PressureSensorForm::getPressSensorData(FacUploadPresSensor x) {
    graph_update(x);  // 图像更新
    save_pressure_data(x);
    calib_process(x);
    test_process(x);
}

void PressureSensorForm::checkbutton(FacButtonState x) {
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

void PressureSensorForm::refreshSn(FacDevInfo data) {
    stringsn = QString::fromUtf8(data.dev_info[0].value_item.tail_sn);
    qDebug() << "dev_info" << data.dev_info[0].value_item.tail_sn;
    qDebug() << "stringsn" << stringsn;
    ui->msgEdit->appendPlainText("芯片存储的尾盖sn:" + stringsn);
}

void PressureSensorForm::refreshBleState(int state) {
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

    pb->reset_all_pb();
    at->resetConnected();
    ui->log->clear();
    isStartcali = 0;

    memset(&cali_result, 0, sizeof(press_calib_data_t));
    memset(&LastCali, 0, sizeof(press_calib_data_t));
    memset(&start_calib_channel, 0, sizeof(start_calib_channel));  // 是否开始刷头200校准

    emit operator_instruct(getIndex());  //核对

    set_independent_state(STATE_INVALID);
    at->sendMac(ui->macInput->text());
    ui->msgEdit->appendPlainText("开始测试");
}

void PressureSensorForm::set_fixture_movement(MODEL_ID_E model, State state, int argument) {
    if (sensor_v.empty()) {
        showlog("模组未初始化,取消发送治具指令");
        return;
    }

    qDebug() << "设置治具参数" << model << state << argument;
    switch (model) {
        case MODEL_ID_Y20P:
            if (getIndex() == 1)
                Y20P_fixture(state, argument);
            break;

        case MODEL_ID_F20: F20_fixture(state, argument); break;

        case MODEL_ID_U7: U7_fixture(state, argument); break;

        case MODEL_ID_P30P:
            // 治具指令跟U7一致
            U7_fixture(state, argument);
            break;
        case MODEL_ID_Y30PS:
            // 治具指令跟U7一致
            U7_fixture(state, argument);
            break;
        case MODEL_ID_Y20PO:

            // 治具指令跟y20po一致
            if (getIndex() == 1)
                Y20PO_fixture(state, argument);
            break;
        default: showlog("无治具配置"); break;
    }
}

void PressureSensorForm::Y20P_fixture(State state, int argument) {
    switch (state) {
        case STATE_CALIB_CH_X:
            switch (sensor_v[argument].para.f_module[0]) {
                case MODULE_BTH:
                    set_independent_state(STATE_CALIB_CH0);
                    send_start_command(COMMAND_ID_BTH_DOWN_200, 0);
                    break;

                case MODULE_MODE_BUTTON: send_start_command(COMMAND_ID_KEY_DOWN_200, 0); break;
            }
            break;

        case STATE_CALIB_CH_X_RESULT:
            switch (sensor_v[argument].para.f_module[0]) {
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

            send_start_command(COMMAND_ID_TRAY_IN, 0);

            send_start_command(COMMAND_ID_FIXED_BLOCK_DOWN, 0);

            break;

        case STATE_SAVE_RESULT:
            set_independent_state(STATE_CALIB_RESULT);

            if (SETTINGS.value("PRESSURE/PressMechine").toInt() == 1) {
                send_start_command(COMMAND_ID_BTH_UP, 0);
                send_start_command(COMMAND_ID_KEY_UP, 0);
                send_start_command(COMMAND_ID_FIXED_BLOCK_UP, 0);
                send_start_command(COMMAND_ID_TRAY_OUT, 0);
            }

            if (SETTINGS.value("PRESSURE/PressMechine").toInt() == 2) {
                send_start_command(COMMAND_ID_BTH_PRESS_UP, 0);
                send_start_command(COMMAND_ID_RESULT, 0);
            }

            break;

        case STATE_TEST_CH_X:
            switch (sensor_v[argument].para.f_module[0]) {
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
            switch (sensor_v[argument].para.f_module[0]) {
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
            switch (sensor_v[argument].para.f_module[0]) {
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
            switch (sensor_v[argument].para.f_module[0]) {
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
void PressureSensorForm::Y20PO_fixture(State state, int argument) {
    if (sensor_v.empty()) {
        showlog("模组未初始化,取消发送治具指令");
        return;
    }
    switch (state) {
        case STATE_CALIB_CH_X:
            switch (sensor_v[argument].para.f_module[0]) {
                case MODULE_BTH:
                    set_independent_state(STATE_CALIB_CH0);
                    send_start_command(COMMAND_ID_BTH_DOWN_200, 0);
                    break;

                case MODULE_MODE_BUTTON: send_start_command(COMMAND_ID_KEY_DOWN_200, 0); break;
            }
            break;

        case STATE_CALIB_CH_X_RESULT:
            switch (sensor_v[argument].para.f_module[0]) {
                case MODULE_BTH:

                    set_independent_state(STATE_CALIB_CH1);
                    send_start_command(COMMAND_ID_BTH_UP, 0);
                    break;

                case MODULE_MODE_BUTTON: send_start_command(COMMAND_ID_KEY_UP, 0); break;
            }

            if (calib_chan + 1 == total_sensor) {
                send_start_command(COMMAND_ID_FIXED_BLOCK_UP, 0);
                // waitWork(3000);
                send_start_command(COMMAND_ID_TRAY_OUT, 0);
            }
            break;

        case STATE_WATI_ASKQR:

            send_start_command(COMMAND_ID_TRAY_IN, 0);

            // waitWork(3000);
            send_start_command(COMMAND_ID_FIXED_BLOCK_DOWN, 0);

            break;

        case STATE_SAVE_RESULT:
            set_independent_state(STATE_CALIB_RESULT);

            if (SETTINGS.value("PRESSURE/PressMechine").toInt() == 1) {
                //没有刷头去掉了
                send_start_command(COMMAND_ID_KEY_UP, 0);
                // waitWork(3000);
                send_start_command(COMMAND_ID_FIXED_BLOCK_UP, 0);
                // waitWork(3000);
                send_start_command(COMMAND_ID_TRAY_OUT, 0);
            }

            if (SETTINGS.value("PRESSURE/PressMechine").toInt() == 2) {
                send_start_command(COMMAND_ID_BTH_PRESS_UP, 0);
                send_start_command(COMMAND_ID_RESULT, 0);
            }

            break;

        case STATE_TEST_CH_X:
            switch (sensor_v[argument].para.f_module[0]) {
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
            switch (sensor_v[argument].para.f_module[0]) {
                case MODULE_BTH: send_start_command(COMMAND_ID_BTH_PRESS_UP, 0); break;
            }
            break;
    }

    // switch (state) {
    // case STATE_CALIB_CH_X:
    //     switch (sensor_v[argument].para.f_module[0]) {
    //     case MODULE_MODE_BUTTON:
    //     case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_O, 0); break;
    //     }
    //     break;

    // case STATE_CALIB_CH_X_RESULT:
    //     switch (sensor_v[argument].para.f_module[0]) {
    //     case MODULE_MODE_BUTTON:
    //     case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
    //     }
    //     break;

    // case STATE_TEST_CH_X:
    //     switch (sensor_v[argument].para.f_module[0]) {
    //     case MODULE_MODE_BUTTON:
    //     case MODULE_POWER_BUTTON:
    //         switch (sensor_v[test_chan].test_status) {
    //         case TEST_KET_NO_CLICK:
    //             send_start_command(COMMAND_ID_FAMA_100_O, 0);
    //             delay_msec(500);
    //             send_start_command(COMMAND_ID_FAMA_100_C, 0);
    //             delay_msec(500);
    //             sensor_v[test_chan].send_state = 1;
    //             break;

    //         case TEST_KET_CLICK:
    //             send_start_command(COMMAND_ID_FAMA_300_O, 0);
    //             delay_msec(500);
    //             send_start_command(COMMAND_ID_FAMA_300_C, 0);
    //             delay_msec(500);
    //             sensor_v[test_chan].send_state = 1;
    //             break;
    //         }
    //         break;
    //     }
    //     break;

    // case STATE_TEST_CH_X_RESULT:
    //     switch (sensor_v[argument].para.f_module[0]) {
    //     case MODULE_MODE_BUTTON:
    //     case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
    //     }
    //     break;

    // case STATE_SAVE_RESULT:
    //     switch (sensor_v[argument].para.f_module[0]) {
    //     case MODULE_MODE_BUTTON:
    //     case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
    //     }
    //     break;

    // default: break;
    // }
}

void PressureSensorForm::U7_fixture(State state, int argument) {
    if (sensor_v.empty()) {
        showlog("模组未初始化,取消发送治具指令");
        return;
    }

    switch (state) {
        case STATE_CALIB_CH_X:
            switch (sensor_v[argument].para.f_module[0]) {
                case MODULE_MODE_BUTTON:
                case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_O, 0); break;
            }
            break;

        case STATE_CALIB_CH_X_RESULT:
            switch (sensor_v[argument].para.f_module[0]) {
                case MODULE_MODE_BUTTON:
                case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
            }
            break;

        case STATE_TEST_CH_X:
            switch (sensor_v[argument].para.f_module[0]) {
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
            switch (sensor_v[argument].para.f_module[0]) {
                case MODULE_MODE_BUTTON:
                case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
            }
            break;

        case STATE_SAVE_RESULT:
            switch (sensor_v[argument].para.f_module[0]) {
                case MODULE_MODE_BUTTON:
                case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
            }
            break;

        default: break;
    }
}

void PressureSensorForm::ui_msg_show(MODEL_ID_E model, State state, int argument) {
    qDebug() << "ui_msg_show参数" << model << state << argument;
    switch (model) {
        case MODEL_ID_Y30PS:
            if (state == STATE_CALIB_CH_X) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[1]);
            } else if (state == STATE_CALIB_CH_X_RESULT) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[2]);
            }

            break;
        case MODEL_ID_Y20PO:
            if (state == STATE_CALIB_CH_X) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[1]);
            } else if (state == STATE_CALIB_CH_X_RESULT) {
                ui->msgEdit->appendPlainText(sensor_v[calib_chan].ui_msg_tip[2]);
            }

            break;

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
        // ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        switch (state) {
            case STATE_IDLE:  // 复位一切
                reset_all();
                calib_vector_init(product_model);
                qDebug() << "sizeof(sensor_v)" << sensor_v.size();

                showlog(ui->macInput->text());
                state = STATE_WATI_CONNECT;
                break;

            case STATE_WATI_CONNECT:  // 设置禁止休眠
                if (at->getConnected()) {
                    ui->msgEdit->appendPlainText("蓝牙连接成功");
                    pb->get_base_info();  // 获取设备信息
                    pb->get_sn(FacDevInfoType_TAIL_SN);
                    state = STATE_DISABLE_SLEEP_CALIB;
                }
                break;

            case STATE_DISABLE_SLEEP_CALIB:  // 禁止休眠
                delay_msec(100);
                if (pb->getDisableSleep()) {
                    delay_msec(200);
                    showlog("已进入禁止休眠");
                    pb->set_press_collect_param(FacSwitch_START);
                    delay_msec(200);
                    state = STATE_WATI_ASKQR;
                } else {
                    delay_msec(100);
                    pb->set_forbid_sleep(FacSwitch_OPEN);
                    delay_msec(200);
                    showlog("再次发送禁止休眠");
                }
                break;

            case STATE_WATI_ASKQR:
                if (get_independent_state() == STATE_CALIB_START) {
                    set_independent_state(STATE_CALIB_STATIC_WAIT);
                    set_fixture_movement(product_model, STATE_WATI_ASKQR, 0);
                    state = STATE_WAIT_STATIC;
                } else if (get_independent_state() == STATE_INVALID) {
                    set_independent_state(STATE_WAIT_START);
                    emit operator_instruct(getIndex());
                    showlog("等待治具就绪");
                }

                break;

            case STATE_WAIT_STATIC:  // 收集无负载时的数据

                if (SETTINGS.value("SYSTEM/PressIndependent").toBool()) {
                    set_independent_state(STATE_CALIB_STATIC_START);
                }
                if (pb->getCollectParam() && isStartcali && get_independent_state() == STATE_CALIB_STATIC_START) {
                    delay_msec(200);
                    if (only_mode == "BTH_ONLY" || only_mode == "KEY_ONLY") {
                        pb->get_press_cali_result();
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
                        pb->get_press_cali_result();
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
                    waittime->setInterval(measure_wait_time);  // 超时判断计时开始
                    waittime->start();
                    showlog("校准超时计时开始,时间：" + QString::number(measure_wait_time));

                    state = STATE_CALIB_CH_X_RESULT;
                } else if (sensor_v[calib_chan].calib_status == CALIB_NOCALIB) {
                    if (LastCali.calib_factor[sensor_v[calib_chan].para.f_module[0]] != 0) {
                        switch (sensor_v[calib_chan].para.f_module[0]) {
                            case MODULE_BTH:
                                sensor_v[calib_chan].calib_result[0] = LastCali.calib_factor[MODULE_BTH];
                                sensor_v[calib_chan].temperature = LastCali.temperature[MODULE_BTH];
                                break;

                            case MODULE_MODE_BUTTON:
                                sensor_v[calib_chan].calib_result[0] = LastCali.calib_factor[MODULE_MODE_BUTTON];
                                break;

                            case MODULE_POWER_BUTTON:
                                sensor_v[calib_chan].calib_result[0] = LastCali.calib_factor[MODULE_POWER_BUTTON];
                                break;
                            case MODULE_INVALID:
                            case MODULE_ASSISTANT_COMPONENT:
                            case MODULE_MAX: break;
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
                    showlog(QString("通道%1已校准完毕").arg(calib_chan));
                    set_fixture_movement(product_model, STATE_CALIB_CH_X_RESULT, calib_chan);
                    if (calib_chan + 1 < total_sensor) {
                        calib_chan++;
                        state = STATE_CALIB_CH_X;
                        showlog(QString("通道%1开始校准").arg(calib_chan));
                    } else {
                        state = STATE_CHECK_CAIL_RESULT;
                        ui->msgEdit->appendPlainText("所有通道校准完毕");
                    }
                    waittime->stop();
                    qDebug() << "waittime->stop();";
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
                    if (sensor_v[chan].para.lower_limit <= sensor_v[chan].calib_result[0] &&
                        sensor_v[chan].para.upper_limit >= sensor_v[chan].calib_result[0]) {
                        showlog(QString("校准值在阈值范围内:%1-%2")
                                    .arg(sensor_v[chan].para.lower_limit)
                                    .arg(sensor_v[chan].para.upper_limit));
                        state = STATE_SEND_CAIL_RESULT;
                    } else {
                        pb->set_press_collect_param(FacSwitch_STOP);
                        delay_msec(200);
                        ui->msgEdit->appendPlainText(QString("校准系数超阈值:%1(%2-%3)")
                                                         .arg(sensor_v[chan].calib_result[0])
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
                        if (ui->mydebug->checkState())
                            state = STATE_SAVE_CAIL_RESULT;
                        else
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
                        pb->get_press_cali_result();
                        delay_msec(200);
                    }
                }
                break;

                //////////////////////////////////////////////////////////////////////////////////////////////
                ///进入测试流程    ////////////////////////////////////////////////////

            case STATE_WAIT_REST:  // 等待复位操作
                if (at->getConnected()) {
                    ui->msgEdit->appendPlainText("蓝牙已重连");

                    pb->reset_all_pb();

                    state = STATE_DISABLE_SLEEP_TEST;
                    // state = STATE_SAVE_RESULT;
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
                            showlog(QString("校准值在阈值范围内:%1-%2")
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
                        "font-size: 33px; background-color: #00FF00; color: black; border: 2px solid #00FF00; "
                        "border-radius: 10px; padding: 10px; text-align: center;");
                } else if ((result == failValue)) {
                    // mes
                    ui->test_result->setText("FAIL");
                    ui->test_result->setStyleSheet(
                        "font-size: 33px; background-color: #FF0000; color: black; border: 2px solid #FF0000; "
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
                save_press_test_data_to_csv(macAddress, cali_result);
                isTestContinue = false;
                // ui->msgEdit->appendPlainText("压感测试完毕");

                pb->set_dev_reset();  // 开始复位牙刷
                delay_msec(50);
                ui->msgEdit->appendPlainText("牙刷已复位");
                closeDongleSerialPort();

                stringsn = "";

                ui->macInput->setDisabled(0);
                ui->getMac->setDisabled(0);
                ui->macInput->clear();
                emit send_end_test(getIndex());
                ui->getMac->clear();

                ui->msgEdit->appendPlainText("流程结束");

                state = STATE_IDLE;

                break;

            default: break;
        }

        //  QCoreApplication::processEvents();
    }
    ui->macInput->setEnabled(true);
}

void PressureSensorForm::on_ClearGraph_clicked() { graph_reset(0); }

void PressureSensorForm::on_GetButton_clicked() {
    pb->get_button_state(1);
    return;
}

void PressureSensorForm::overTask() { on_end_clicked(); }

void PressureSensorForm::on_button_get_calib_factor_clicked() {
    delay_msec(200);
    pb->get_press_cali_result();
    delay_msec(200);
}

uint8_t PressureSensorForm::CheckNfcData() {
    uint8_t ret = 0;
    QString ReadNfcData = ReadNfcDataProcess();
    if (ReadNfcData == 0)
        return 0;
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
    showlog("model:" + model);
    showlog("supid:" + supid);
    showlog("fix_C:" + C);
    showlog("sn:" + sn);
    showlog("fix_E:" + E);
    showlog("mac:" + mac);

    QString hexString;
    QString text = stringsn;
    QString truncatedSN = text.left(8);

    QString value = SETTINGS.value("SUBPID/" + truncatedSN, "SUBPID_ERRO").toString();
    showlog("truncatedSN：" + truncatedSN + "匹配到的subpid：" + value);

    for (int i = 0; i < text.length(); ++i) {
        hexString += QString("%1").arg(text[i].toLatin1(), 2, 16, QChar('0')).toUpper();
    }
    showlog("转化为:" + hexString);

    // 比对型号
    showlog("当前型号为：" + readProduct);
    if (readProduct == "U7P" && model == "033BD2023668772001004800324F3130") {
        showlog("Check:U7P");
        ret = 1;
    } else if (readProduct == "U7" && model == "033BD2023668772001004800324F3045") {
        ret = 1;
        showlog("Check:U7");
    } else {
        showlog("NFC标签:型号编码错误,error model:" + model);
        return ret;
    }

    // 对比supid
    if (value == supid && ret == 1) {
        showlog("supid 校验通过");
        ret = 2;
    } else {
        showlog("NFC标签:supid错误,error:" + supid);
        showlog("correct:" + value);
        return ret;
    }

    if (C == "810800272000141785911410" && ret == 2) {
        ret = 3;
        showlog("C 校验通过");
    } else {
        showlog("NFC标签:型号编码错误,fix_C:" + C);
        return ret;
    }

    if (sn == hexString && ret == 3) {
        showlog("sn 校验通过");
        ret = 4;
    } else {
        showlog("NFC标签:sn编码错误,error:" + sn);
        showlog("hexString:" + hexString);
        return ret;
    }

    if (E == "170102910B0101010A06" && ret == 4) {
        showlog("E 校验通过");
        ret = 5;
    } else {
        showlog("NFC标签:fix_e编码错误,error:" + E);
        return ret;
    }

    if (mac == ui->macInput->text().remove(':').right(12) && ret == 5) {
        showlog("mac 校验通过");
        ret = 6;
    } else {
        showlog("NFC标签:mac编码错误,error:" + mac);
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
            qDebug() << "nfc烧录器读取成功,需要" << name;
            QString buffStr = QString::fromLatin1(reinterpret_cast<const char*>(buff_1), 8);
            if (buffStr == name) {
                showlog("找到NFC设备端口号" + QString::number(k));
                return k;
            } else {
                qDebug() << "设备名字为" << buffStr;
            }
        }
    }
    showlog("没找到设备端口号，用第一个端口");
    return 100;
}

QString PressureSensorForm::ReadNfcDataProcess() {
    HANDLE icdev = (HANDLE)-1;
    int st = -1;
    unsigned char _Snr[100] = "\0";
    unsigned char szSnr[100] = "\0";
    unsigned int SnrLen = 0;
    QString ReadNfcData = "";
    int dataSize = 61;
    unsigned char rdata[100] = "\0";
    unsigned char rdatahex[100] = "\0";

    int nfcport = findNfcDevicePort(ui->NfcComboBox->currentText());
    icdev = dc_init(nfcport, 115200);
    if ((intptr_t)icdev <= 0) {
        showlog("初始化nfc接口失败!");
        return 0;
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

        return 0;
    } else {
        showlog("nfc卡查询成功");
        memset(szSnr, 0x00, sizeof(szSnr));
        hex_a(_Snr, szSnr, SnrLen);
        std::string str1 = (char*)szSnr;
        showlog("卡的序列号为" + QString::fromStdString(str1));
    }

    for (int i = 4; i * 4 < dataSize; i += 4) {  // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0) {
            // ui->msgEdit->appendPlainText("dc_read Error!");
            showlog("nfc信息读取失败");
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
            showlog("nfc信息读取失败");
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

    showlog("nfc内容为：" + ReadNfcData);

    return ReadNfcData;
}

void PressureSensorForm::refreshBaseData(FacGetDevBaseInfo data) {
    readProduct = data.product_name;
    qDebug() << "refreshBaseData";
    showlog("获取到当前型号为：" + readProduct);
    showlog(data.product_name);
}

void PressureSensorForm::on_TestButtonNFC_clicked() { QString ReadNfcData = ReadNfcDataProcess(); }

void PressureSensorForm::on_TestButton1_clicked() {
    // pb->get_base_info();  // 获取设备信息

    CheckNfcData();
}

void PressureSensorForm::on_SendCalib_clicked() {
    return;

    press_calib_data_t cali_result;
    cali_result.calib_factor[MODULE_BTH] = 2666;
    cali_result.calib_factor[MODULE_POWER_BUTTON] = 2666;
    cali_result.calib_factor[MODULE_MODE_BUTTON] = 2666;
    cali_result.temperature[MODULE_BTH] = 2666;

    pb->set_press_cali_result(cali_result);
}
