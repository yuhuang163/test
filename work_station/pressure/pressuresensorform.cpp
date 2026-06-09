#include "pressuresensorform.h"

#include <QHBoxLayout>
#include <QMessageBox>
#include <QSerialPortInfo>
#include <QVBoxLayout>

#include "AbIni.h"
#include "fixture_uart.h"
#include "ui_pressuresensorform.h"

extern "C" // 由于是C版的dll文件，在C++中引入其头文件要加extern "C" {},注意
{
#include "lib/nfc/dcrf32.h"
}

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

PressureSensorForm::PressureSensorForm(int index, QWidget* parent) : test_base(parent), ui(new Ui::PressureSensorForm) {
    m_index = index;
    qDebug() << "当前索引" << getIndex();
    pack.mechines = getIndex();

    dongleOutTime = 1; // 太快会死锁
    upperComputerVer = PRESSURE_VER;

    ui->setupUi(this);
    updateMainStyle("Ubuntu.qss");
    ui->bleStatusLabel->setText("蓝牙连接：<font color='red'>失败</font>");
    ui->uartStatusLabel->setText("串口连接：<font color='red'>失败</font>");
    ui->macLabel->setText("蓝牙mac:                        ");
    ui->frame_rate->setText("数据帧率:            ");

    // //治具串口不可见
    // for (auto i = 0; i < ui->horizontalLayout_14->count(); i++) {
    //     QWidget* w = ui->horizontalLayout_14->itemAt(i)->widget();
    //     if (w != nullptr) {
    //         w->setVisible(false);
    //     }
    // }

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

    pressTestTime = SETTINGS.value("PRESSURE/TestTime").toInt();
    measure_wait_time = SETTINGS.value("PRESSURE/CaliTime").toInt();
    pack.product = SETTINGS.value("Mes/Product_Name").toString();
    product_model_init(pack.product);

    graph_init(product_model);

    only_mode = ONLY_SET_E(SETTINGS.value("PRESSURE/Module", 1).toInt());

    AdcShakeValue = SETTINGS.value("PRESSURE/ADCShakeValue", 100).toInt();
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
    ui->tabWidget->setCurrentIndex(0); // 设置当前页为第一页
    testResultTableInit();

    // connect(this, SIGNAL(dataReady()), this, SLOT(updateGraphs()));
    // // 创建一个 QTimer 对象
    // QTimer* graphtimer = new QTimer(this);

    // // 设置定时器的间隔时间，这里设置为 1000 毫秒（即 1 秒），你可以根据需要调整
    // graphtimer->setInterval(100);

    // // 连接定时器的 timeout 信号到 updateGraphs() 槽函数
    // connect(graphtimer, &QTimer::timeout, this, &PressureSensorForm::updateGraphs);

    // // 启动定时器
    // graphtimer->start();
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
    } else if (model == MODEL_ID_Y20) {
        chnal_num = 1;
    } else if (model == MODEL_ID_F20) {
        chnal_num = 2;
    } else if (model == MODEL_ID_U7) {
        chnal_num = 1;
    } else if (model == MODEL_ID_Y21) {
        chnal_num = 1;
    } else if (model == MODEL_ID_P30P) {
        chnal_num = 1;
    } else if (model == MODEL_ID_Y30S) {
        chnal_num = 2;
    } else if (model == MODEL_ID_Y20PS) {
        chnal_num = 2;
    } else if (model == MODEL_ID_Y30P) {
        chnal_num = 1;
    }

    for (int i = 0; i < chnal_num; i++) {
        QCustomPlot* plot_adc = new QCustomPlot;
        QCustomPlot* plot_value = new QCustomPlot;

        plot_adc->legend->setVisible(true);                           // 设置图例可见
        plot_adc->xAxis->setRange(0, 1000);                           // 设置 X 轴范围为 0 到 1000
        plot_adc->yAxis->setRange(0, 1000);                           // 设置 Y 轴范围为 0 到 1000
        plot_adc->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom); // 启用范围拖动和缩放
        plot_adc->addGraph();                                         // 添加图层
        plot_adc->graph(0)->setPen(QPen(Qt::red));                    // 设置图层画笔为红色
        plot_adc->graph(0)->setBrush(Qt::NoBrush);                    // 不填充颜色
        plot_adc->graph(0)->setAntialiased(true);                     // 不抗锯齿

        plot_adc->graph(0)->setName("ADC值"); // 设置图层名称
        plot_adc->xAxis->setLabel("时间（ms）");
        plot_adc->yAxis->setLabel("adc值");
        graph_adc_vector.push_back(plot_adc);

        // 创建压力值曲线图
        plot_value->legend->setVisible(true);                           // 设置图例可见
        plot_value->xAxis->setRange(0, 1000);                           // 设置 X 轴范围为 0 到 1000
        plot_value->yAxis->setRange(0, 1000);                           // 设置 Y 轴范围为 0 到 1000
        plot_value->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom); // 启用范围拖动和缩放
        plot_value->addGraph();                                         // 添加图层
        plot_value->graph(0)->setPen(QPen(Qt::blue));                   // 设置图层画笔为蓝色
        plot_value->graph(0)->setBrush(QBrush(Qt::NoBrush));            // 使用蓝色填充
        plot_value->graph(0)->setAntialiased(true);                     // 不抗锯齿
        plot_value->graph(0)->setName("压力值");                        // 设置图层名称

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
    // 先清空容器里的元素
    if (!sensor_v.empty()) {
        // 清空 sensor_v
        sensor_v.clear();
        qDebug() << "sensor_v has been cleared.";
    } else {
        qDebug() << "sensor_v is already empty.";
    }

    qDebug() << "初始化压感模组容器";

    if (model == MODEL_ID_Y30P) {
        ndt_sensor_cali* y20p_ch0_vector = new ndt_sensor_cali; // 动态分配内存
        y20p_ch0_vector->ui_msg_tip[0] = donotmove;
        y20p_ch0_vector->ui_msg_tip[1] = brush_placing_200_weights;
        y20p_ch0_vector->ui_msg_tip[2] = pack_weights;
        y20p_ch0_vector->ui_msg_err[0] = "电机错误码为：";
        y20p_ch0_vector->ui_msg_test[0] = "人员：请放50g砝码";
        y20p_ch0_vector->ui_msg_test[1] = "人员：请放250g砝码";
        y20p_ch0_vector->ui_msg_test[2] = "人员：请放450g砝码";

        y20p_ch0_vector->para.test_grams[0] = 200;
        y20p_ch0_vector->para.test_grams[1] = 400;

        y20p_ch0_vector->para.test_tolerance[0] = 50;
        y20p_ch0_vector->para.test_tolerance[1] = 50;
        y20p_ch0_vector->para.test_count[0] = 300;
        y20p_ch0_vector->para.test_count[1] = 300;

        y20p_ch0_vector->para.adc_threshold = SETTINGS.value("PRESSURE/BrushThreshold", 350).toInt();
        y20p_ch0_vector->para.count_threshold = SETTINGS.value("PRESSURE/BrushThresholdCount", 300).toInt();

        y20p_ch0_vector->para.f_module[0] = MODULE_BTH;

        y20p_ch0_vector->para.lower_limit = SETTINGS.value("PRESSURE/bth_lower", "1000").toInt();
        y20p_ch0_vector->para.upper_limit = SETTINGS.value("PRESSURE/bth_upper", "3000").toInt();
        sensor_v.push_back(y20p_ch0_vector);
        sensor_v[0]->Sensor_cali_Init(CAL_CHANNEL_Y30P_CH0);
    }
    if (model == MODEL_ID_F20) {
        ndt_sensor_cali* y20p_ch0_vector = new ndt_sensor_cali; // 动态分配内存
        y20p_ch0_vector->ui_msg_tip[0] = donotmove;
        y20p_ch0_vector->ui_msg_tip[1] = brush_placing_200_weights;
        y20p_ch0_vector->ui_msg_tip[2] = pack_weights;
        y20p_ch0_vector->ui_msg_err[0] = "电机错误码为：";
        y20p_ch0_vector->ui_msg_test[0] = "人员：请放50g砝码";
        y20p_ch0_vector->ui_msg_test[1] = "人员：请放250g砝码";
        y20p_ch0_vector->ui_msg_test[2] = "人员：请放450g砝码";

        ndt_sensor_cali* y20p_ch1_vector = new ndt_sensor_cali; // 动态分配内存
        y20p_ch1_vector->ui_msg_tip[0] = donotmove;
        y20p_ch1_vector->ui_msg_tip[1] = botton_placing_200_weights;
        y20p_ch1_vector->ui_msg_tip[2] = "拿走按键200g砝码";
        y20p_ch1_vector->ui_msg_err[0] = "按键错误码为：";
        y20p_ch1_vector->ui_msg_test[0] = "人员：按键请放230g砝码";

        // y20p_ch1_vector->sensor_test_status_set(TEST_NOTEST);

        // test_data_init
        // y20p_ch0_vector.test_time = 2;
        y20p_ch0_vector->para.test_grams[0] = 200;
        y20p_ch0_vector->para.test_grams[1] = 400;

        y20p_ch0_vector->para.test_tolerance[0] = 50;
        y20p_ch0_vector->para.test_tolerance[1] = 50;
        y20p_ch0_vector->para.test_count[0] = 300;
        y20p_ch0_vector->para.test_count[1] = 300;

        y20p_ch0_vector->para.adc_threshold = SETTINGS.value("PRESSURE/BrushThreshold", 350).toInt();
        y20p_ch0_vector->para.count_threshold = SETTINGS.value("PRESSURE/BrushThresholdCount", 300).toInt();

        y20p_ch1_vector->para.adc_threshold = SETTINGS.value("PRESSURE/ButtonThreshold", 30).toInt();
        y20p_ch1_vector->para.count_threshold = SETTINGS.value("PRESSURE/ButtonThresholdCount", 400).toInt();

        y20p_ch0_vector->para.f_module[0] = MODULE_BTH;
        y20p_ch1_vector->para.f_module[0] = MODULE_MODE_BUTTON;

        // y20p_ch1_vector.test_time = 1;
        y20p_ch1_vector->para.test_grams[0] = 200;
        y20p_ch1_vector->para.test_tolerance[0] = 50;
        y20p_ch1_vector->para.test_count[0] = 300;

        y20p_ch0_vector->para.lower_limit = SETTINGS.value("PRESSURE/bth_lower", "1000").toInt();
        y20p_ch0_vector->para.upper_limit = SETTINGS.value("PRESSURE/bth_upper", "3000").toInt();

        y20p_ch1_vector->para.lower_limit = SETTINGS.value("PRESSURE/model_button_lower", "1000").toInt();
        y20p_ch1_vector->para.upper_limit = SETTINGS.value("PRESSURE/model_button_upper", "3000").toInt();

        if (only_mode == KEY_ONLY) {
            sensor_v.push_back(y20p_ch1_vector);
            sensor_v[0]->Sensor_cali_Init(CAL_CHANNEL_F20_CH1);
            showlog("当前为单按键");

        } else if (only_mode == BTH_ONLY) {
            showlog("当前为单电机");
            sensor_v.push_back(y20p_ch0_vector);
            sensor_v[0]->Sensor_cali_Init(CAL_CHANNEL_F20_CH0);
        } else {
            sensor_v.push_back(y20p_ch0_vector);
            sensor_v.push_back(y20p_ch1_vector);
            sensor_v[0]->Sensor_cali_Init(CAL_CHANNEL_F20_CH0);

            sensor_v[1]->Sensor_cali_Init(CAL_CHANNEL_F20_CH1);
        }
    }
    if (model == MODEL_ID_Y20) {
        ndt_sensor_cali* y20_ch0_vector = new ndt_sensor_cali; // 动态分配内存
        y20_ch0_vector->ui_msg_tip[0] = donotmove;
        y20_ch0_vector->ui_msg_tip[1] = brush_placing_200_weights;
        y20_ch0_vector->ui_msg_tip[2] = pack_weights;
        y20_ch0_vector->ui_msg_err[0] = "电机错误码为：";
        y20_ch0_vector->ui_msg_test[0] = "人员：请放330g砝码";
        y20_ch0_vector->ui_msg_test[1] = "人员：请放350g砝码";
        y20_ch0_vector->ui_msg_test[2] = "人员：请放520g砝码";
        y20_ch0_vector->para.f_module[0] = MODULE_BTH;
        sensor_v.push_back(y20_ch0_vector);
    }
    if (model == MODEL_ID_Y20P) {
        ndt_sensor_cali* y20p_ch0_vector = new ndt_sensor_cali; // 动态分配内存
        y20p_ch0_vector->ui_msg_tip[0] = donotmove;
        y20p_ch0_vector->ui_msg_tip[1] = brush_placing_200_weights;
        y20p_ch0_vector->ui_msg_tip[2] = pack_weights;
        y20p_ch0_vector->ui_msg_err[0] = "电机错误码为：";
        y20p_ch0_vector->ui_msg_test[0] = "人员：请放50g砝码";
        y20p_ch0_vector->ui_msg_test[1] = "人员：请放350g砝码";
        y20p_ch0_vector->ui_msg_test[2] = "人员：请放450g砝码";

        ndt_sensor_cali* y20p_ch1_vector = new ndt_sensor_cali; // 动态分配内存
        y20p_ch1_vector->ui_msg_tip[0] = donotmove;
        y20p_ch1_vector->ui_msg_tip[1] = botton_placing_200_weights;
        y20p_ch1_vector->ui_msg_tip[2] = "拿走按键230g砝码";
        y20p_ch1_vector->ui_msg_err[0] = "按键错误码为：";
        y20p_ch1_vector->ui_msg_test[0] = "人员：按键请放230g砝码";

        y20p_ch1_vector->sensor_test_status_set(TEST_NOTEST);

        // test_data_init
        // y20p_ch0_vector.test_time = 2;
        y20p_ch0_vector->para.test_grams[0] = 200;
        y20p_ch0_vector->para.test_grams[1] = 400;

        y20p_ch0_vector->para.test_tolerance[0] = 50;
        y20p_ch0_vector->para.test_tolerance[1] = 50;
        y20p_ch0_vector->para.test_count[0] = 300;
        y20p_ch0_vector->para.test_count[1] = 300;

        // y20p_ch0_vector.para.adc_threshold = ADC_THRESHOLD_Y20P_BTH;
        // y20p_ch1_vector.para.adc_threshold = ADC_THRESHOLD_Y20P_KEY;

        // y20p_ch0_vector.para.count_threshold = COUNT_THRESHOLD_Y20P_BTH;
        // y20p_ch1_vector.para.count_threshold = COUNT_THRESHOLD_Y20P_KEY;

        y20p_ch0_vector->para.adc_threshold = SETTINGS.value("PRESSURE/BrushThreshold", 350).toInt();
        y20p_ch0_vector->para.count_threshold = SETTINGS.value("PRESSURE/BrushThresholdCount", 300).toInt();

        y20p_ch1_vector->para.adc_threshold = SETTINGS.value("PRESSURE/ButtonThreshold", 30).toInt();
        y20p_ch1_vector->para.count_threshold = SETTINGS.value("PRESSURE/ButtonThresholdCount", 400).toInt();

        y20p_ch0_vector->para.f_module[0] = MODULE_BTH;
        y20p_ch1_vector->para.f_module[0] = MODULE_MODE_BUTTON;

        // y20p_ch1_vector.test_time = 1;
        y20p_ch1_vector->para.test_grams[0] = 200;
        y20p_ch1_vector->para.test_tolerance[0] = 50;
        y20p_ch1_vector->para.test_count[0] = 300;

        y20p_ch0_vector->para.lower_limit = SETTINGS.value("PRESSURE/bth_lower", "1000").toInt();
        y20p_ch0_vector->para.upper_limit = SETTINGS.value("PRESSURE/bth_upper", "3000").toInt();

        y20p_ch1_vector->para.lower_limit = SETTINGS.value("PRESSURE/model_button_lower", "1000").toInt();
        y20p_ch1_vector->para.upper_limit = SETTINGS.value("PRESSURE/model_button_upper", "3000").toInt();

        qDebug() << " y20p_ch1_vector->para.lower_limit" << y20p_ch1_vector->para.lower_limit;

        sensor_v.push_back(y20p_ch0_vector);
        sensor_v.push_back(y20p_ch1_vector);
        qDebug() << "CAL_CHANNEL_Y20_CH0";
        sensor_v[0]->Sensor_cali_Init(CAL_CHANNEL_Y20_CH0);
        qDebug() << "CAL_CHANNEL_Y20_CH0";
        sensor_v[1]->Sensor_cali_Init(CAL_CHANNEL_Y20_CH1);
        qDebug() << "压感结果2" << sensor_v[0]->para.lower_limit << sensor_v[1]->para.lower_limit;

    } else if (model == MODEL_ID_Y30S) {
        ndt_sensor_cali* Y30S_ch0_vector = new ndt_sensor_cali; // 动态分配内存
        Y30S_ch0_vector->ui_msg_tip[0] = donotmove;
        Y30S_ch0_vector->ui_msg_tip[1] = "按键放400g砝码";
        Y30S_ch0_vector->ui_msg_tip[2] = "拿走砝码";

        Y30S_ch0_vector->ui_msg_test[0] = "测试200g不触发";
        Y30S_ch0_vector->ui_msg_test[1] = "测试400g触发";

        Y30S_ch0_vector->ui_msg_err[0] = "按键错误码为：";

        Y30S_ch0_vector->para.adc_threshold = SETTINGS.value("PRESSURE/ButtonThreshold").toInt();

        Y30S_ch0_vector->para.count_threshold = SETTINGS.value("PRESSURE/ButtonThresholdCount").toInt();

        Y30S_ch0_vector->para.f_module[0] = MODULE_MODE_BUTTON;
        Y30S_ch0_vector->para.f_module[1] = MODULE_ASSISTANT_COMPONENT;

        Y30S_ch0_vector->para.lower_limit = SETTINGS.value("PRESSURE/model_button_lower", "2500").toInt();
        Y30S_ch0_vector->para.upper_limit = SETTINGS.value("PRESSURE/model_button_upper", "4500").toInt();

        sensor_v.push_back(Y30S_ch0_vector);

        sensor_v[0]->Sensor_cali_Init(CAL_CHANNEL_Y30S_CH0);

    } else if (model == MODEL_ID_Y20PS) {
        ndt_sensor_cali* Y20PS_ch0_vector = new ndt_sensor_cali; // 动态分配内存
        Y20PS_ch0_vector->ui_msg_tip[0] = donotmove;
        Y20PS_ch0_vector->ui_msg_tip[1] = "按键放砝码";
        Y20PS_ch0_vector->ui_msg_tip[2] = "拿走砝码";

        Y20PS_ch0_vector->ui_msg_test[0] = "测试300g不触发";
        Y20PS_ch0_vector->ui_msg_test[1] = "测试500g触发";

        Y20PS_ch0_vector->ui_msg_err[0] = "按键错误码为：";

        Y20PS_ch0_vector->para.adc_threshold = SETTINGS.value("PRESSURE/ButtonThreshold").toInt();

        Y20PS_ch0_vector->para.count_threshold = SETTINGS.value("PRESSURE/ButtonThresholdCount").toInt();

        Y20PS_ch0_vector->para.f_module[0] = MODULE_MODE_BUTTON;
        Y20PS_ch0_vector->para.f_module[1] = MODULE_ASSISTANT_COMPONENT;
        Y20PS_ch0_vector->para.lower_limit = SETTINGS.value("PRESSURE/model_button_lower", "2500").toInt();
        Y20PS_ch0_vector->para.upper_limit = SETTINGS.value("PRESSURE/model_button_upper", "4500").toInt();

        sensor_v.push_back(Y20PS_ch0_vector);

        sensor_v[0]->Sensor_cali_Init(CAL_CHANNEL_Y20PS_CH0);

    }

    else if (model == MODEL_ID_U7) {
        ndt_sensor_cali* u7_ch0_vector = new ndt_sensor_cali; // 动态分配内存
        u7_ch0_vector->ui_msg_tip[0] = donotmove;
        u7_ch0_vector->ui_msg_tip[1] = "按键放300g砝码";
        u7_ch0_vector->ui_msg_tip[2] = "拿走砝码";

        u7_ch0_vector->ui_msg_test[0] = "测试100g不触发";
        u7_ch0_vector->ui_msg_test[1] = "测试300g触发";

        u7_ch0_vector->ui_msg_err[0] = "按键错误码为：";

        u7_ch0_vector->para.adc_threshold = SETTINGS.value("PRESSURE/ButtonThreshold").toInt();

        u7_ch0_vector->para.count_threshold = SETTINGS.value("PRESSURE/ButtonThresholdCount").toInt();

        u7_ch0_vector->para.f_module[0] = MODULE_MODE_BUTTON;

        u7_ch0_vector->para.lower_limit = SETTINGS.value("PRESSURE/model_button_lower", "2500").toInt();
        u7_ch0_vector->para.upper_limit = SETTINGS.value("PRESSURE/model_button_upper", "4500").toInt();

        sensor_v.push_back(u7_ch0_vector);

        sensor_v[0]->Sensor_cali_Init(CAL_CHANNEL_U7_CH0);

    } else if (model == MODEL_ID_P30P) {
        ndt_sensor_cali* p30p_ch0_vector = new ndt_sensor_cali; // 动态分配内存
        p30p_ch0_vector->ui_msg_tip[0] = donotmove;
        p30p_ch0_vector->ui_msg_tip[1] = "按键放300g砝码";
        p30p_ch0_vector->ui_msg_tip[2] = "拿走砝码";

        p30p_ch0_vector->ui_msg_err[0] = "电机错误码为：";

        p30p_ch0_vector->ui_msg_test[0] = "测试100g不触发";
        p30p_ch0_vector->ui_msg_test[1] = "测试300g触发";

        p30p_ch0_vector->para.adc_threshold = SETTINGS.value("PRESSURE/ButtonThreshold").toInt();

        p30p_ch0_vector->para.count_threshold = SETTINGS.value("PRESSURE/ButtonThresholdCount").toInt();

        p30p_ch0_vector->para.f_module[0] = MODULE_POWER_BUTTON;

        p30p_ch0_vector->para.lower_limit = SETTINGS.value("PRESSURE/power_button_lower", "500").toInt();
        p30p_ch0_vector->para.upper_limit = SETTINGS.value("PRESSURE/power_button_upper", "4500").toInt();

        sensor_v.push_back(p30p_ch0_vector);

        sensor_v[0]->Sensor_cali_Init(CAL_CHANNEL_P30P_CH0);
    }

    total_sensor = static_cast<int>(sensor_v.size());
    qDebug() << "model:" << model << "压感模组通道总数为：" << total_sensor;

    for (int i = 0; i < total_sensor; i++)
        connect(sensor_v[i], &ndt_sensor_cali::send_press_cali_msg, this, &PressureSensorForm::showlog);
}
void PressureSensorForm::getTestValue(const int mechines, const QString value) {
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
    } else if (pack.factory == "hz") {
        if (mechines != getIndex()) {
            return;
        }
        const QString snFromMes = value.trimmed();
        mesmacAddress = parseMacFromSn(snFromMes);
        if (mesmacAddress.isEmpty()) {
            showlog(QStringLiteral("MES 返回 SN 解析 MAC 失败"));
            showlog(value);
            return;
        }
        ui->macInput->setText(mesmacAddress);
        showlog(QStringLiteral("MES SN 解析 MAC 成功: ") + mesmacAddress);
        on_macInput_returnPressed();
    } else {
        if (mechines == getIndex()) {
            mesmacAddress = value;
            ui->macInput->setText(mesmacAddress);
            on_macInput_returnPressed();
        }
    }

    // bindingMacSn(mesmacAddress, ui->getMac->text());//获取测试数据不要绑定测试mac——sn
}

void PressureSensorForm::refreshPressCalibData(ProtocolPressCalibResultData x) {
    if (x.brushHeadAdc != 0) {
        showlog("读取到的电机校准系数" + QString::number(x.brushHeadAdc));
        LastCali.calib_factor[MODULE_BTH] = x.brushHeadAdc;
        qDebug() << "获取到电机校准系数：" << LastCali.calib_factor[MODULE_BTH];
    }
    if (x.modeButtonAdc != 0) {
        showlog("读取到的模式按键校准系数" + QString::number(x.modeButtonAdc));
        LastCali.calib_factor[MODULE_MODE_BUTTON] = x.modeButtonAdc;
        qDebug() << "获取到模式按键校准系数：" << LastCali.calib_factor[MODULE_MODE_BUTTON];
    }
    if (x.powerButtonAdc != 0) {
        showlog("读取到的电源按键校准系数" + QString::number(x.powerButtonAdc));
        LastCali.calib_factor[MODULE_POWER_BUTTON] = x.powerButtonAdc;
        qDebug() << "获取到电源按键校准系数：" << LastCali.calib_factor[MODULE_MODE_BUTTON];
    }
    if (x.temperature != 0) {
        showlog("读取到的温度校准系数" + QString::number(x.temperature));
        LastCali.temperature[MODULE_BTH] = x.temperature;
        qDebug() << "获取到温度系数：" << LastCali.calib_factor[MODULE_MODE_BUTTON];
    }

    if (x.assistantComponent != 0) {
        showlog("读取到的辅助元器件校准系数" + QString::number(x.assistantComponent));
        LastCali.calib_factor[MODULE_ASSISTANT_COMPONENT] = x.assistantComponent;
        qDebug() << "辅助元器件" << LastCali.calib_factor[MODULE_ASSISTANT_COMPONENT];
    }

    //校准值是0就不比对
    if (
        /* (cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT] == 0 ||
          x.assistant_component == (uint32_t)cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT]) &&*/
        (cali_result.calib_factor[MODULE_BTH] == 0 ||
         x.brushHeadAdc == (uint32_t)cali_result.calib_factor[MODULE_BTH]) &&
        (cali_result.calib_factor[MODULE_MODE_BUTTON] == 0 ||
         x.modeButtonAdc == (uint32_t)cali_result.calib_factor[MODULE_MODE_BUTTON]) &&
        (cali_result.calib_factor[MODULE_POWER_BUTTON] == 0 ||
         x.powerButtonAdc == (uint32_t)cali_result.calib_factor[MODULE_POWER_BUTTON]) &&
        (x.temperature == (uint32_t)cali_result.temperature[MODULE_BTH] ||
         x.temperature == (uint32_t)cali_result.temperature[MODULE_MODE_BUTTON])) {
        qDebug() << "校准数据核对成功";
        is_calib_suc = 1;
    }
}

PressureSensorForm::~PressureSensorForm() {
    delete ui;
}

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

void PressureSensorForm::send_frame_rate(QString data) {
    ui->frame_rate->setText("数据帧率: " + data);
}

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
    // waitWork(3000);
    FixturePacketData packet_data;

    packet_data.machine_command_id = command_id; //命令治具状态
    packet_data.argument = argument;
    packet_data.machineNumber = getIndex() - 1; //指令的数组从0开始跑的

    qDebug() << "getIndex():" << getIndex();

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
                    << "电机校准系数或错误码"
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
            rowData << QString::number(sensor_v[test_chan]->test_result[0]);
        } else if (result == failValue) {
            // 写入错误码
            for (int i = 0; i < total_sensor; i++) { //双通道共用一个校准结果
                rowData << QString::number(sensor_v[i]->err[0]);
            }
            rowData << QString("测试失败：%1").arg(sensor_v[test_chan]->test_result[0]);
        }
        stream << rowData.join(",") << "\n";

        file.close();
        // showlog("文件保存到" + filePath);

        qDebug() << "文件保存到" << filePath;
    } else {
        showlog("文件没关或者其他问题");
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
    applyAdaptiveV3ProductBySn(ui->getMac);

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
    getMac(ui->getMac->text());            // 文件获取
    processInspection(ui->getMac->text()); // 站前检测
    processGetMesTestValue();              // mes获取
}

void PressureSensorForm::processInspection(QString inputSnText) {
    if (inputSnText != "" || !ui->isusemes->checkState()) {
        if (ui->isusemes->checkState()) {
            showlog("正在进行站前检测");
            pack.sn = inputSnText;
            pack.mechines = getIndex();
            pack.is_hq_send_mac = 0;
            pack.instruct_num = "079";
            emit send_process_inspection(pack);
        }
    } else {
        showlog("SN比对错误");
    }

    if (!ui->isusemes->checkState()) // 离线
    {
        ui->mes_state->setText("MES");
        ui->mes_state->setStyleSheet("font-size: 33px; background-color: #FFFF00; color: black; border: 2px solid "
                                     "#FF0000; border-radius: 10px; padding: 10px; text-align: center; ");
    }
}

void PressureSensorForm::processGetMesTestValue() {
    if (pack.factory == "hz") {
        pack.sn = ui->getMac->text();
        pack.mechines = getIndex();
        getTestValue(getIndex(), pack.sn.trimmed());
        return;
    }
    if (ui->isformmes->checkState()) {
        pack.sn = ui->getMac->text();
        pack.is_hq_send_mac = 1;
        pack.mechines = getIndex();
        pack.instruct_num = "079";
        emit send_mes_test_value(pack);
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
        protocolManager.set(DeviceCmd::DevReset); // 开始复位设备
        delay_msec(50);
        showlog("设备已复位");
    }

    if (dongleSerialPort->isOpen()) {
        closeDongleSerialPort();
    }

    // set_independent_state(STATE_CALIB_RESULT);
    // set_fixture_movement(product_model, STATE_SAVE_RESULT, 0);
    ui->getMac->setFocus();
}

void PressureSensorForm::savePressDataToLocalFolder(const ProtocolPressSampleData& x, bool appHeader) {
    // 获取当前日期
    QDate currentDate = QDate::currentDate();
    QString dateFolder = currentDate.toString("yyyy-MM-dd"); // 获取当前日期字符串作为文件夹名

    // 构建文件夹路径：在 "测试结果" 下新建 "压感数据" 文件夹，再在其中创建日期文件夹
    QString folderPath = "D:/测试结果/压感数据/" + dateFolder;

    // 如果 "压感数据" 文件夹或日期文件夹不存在，则创建它们
    if (!QDir(folderPath).exists()) {
        QDir().mkpath(folderPath);
    }

    // 使用 macAddress 来命名文件
    QString fileName = macAddress.remove(":") + "_压感数据报告.csv"; // 使用 macAddress 作为文件名
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
                << "电机ADC"
                << "电机压力"
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
    const int sampleCount = qMin(x.adcValues.size(), x.valueValues.size()) / 4;
    for (int i = 0; i < sampleCount; i++) {
        const int base = i * 4;
        QStringList dataList;
        dataList << timestamp << macAddress << QString::number(x.adcValues[base]) << QString::number(x.valueValues[base])
                 << QString::number(x.adcValues[base + 1]) << QString::number(x.valueValues[base + 1])
                 << QString::number(x.adcValues[base + 2]) << QString::number(x.valueValues[base + 2])
                 << QString::number(x.adcValues[base + 3]) << QString::number(x.valueValues[base + 3]);

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
    } else if (model == "Y20") {
        product_model = MODEL_ID_Y20;
    } else if (model == "F20") {
        product_model = MODEL_ID_F20;
    } else if (model == "U7" || model == "U7P") {
        product_model = MODEL_ID_U7;
    } else if (model == "Y21") {
        product_model = MODEL_ID_Y21;
    } else if (model == "P30P") {
        product_model = MODEL_ID_P30P;
    } else if (model == "Y30S") {
        product_model = MODEL_ID_Y30S;
    } else if (model == "Y20PS") {
        product_model = MODEL_ID_Y20PS;
    } else if (model == "Y30P") {
        product_model = MODEL_ID_Y30P;
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
    if (product_model == MODEL_ID_Y30S || product_model == MODEL_ID_Y20PS) {
        need_sensor_data = 2;
    } else {
        need_sensor_data = 1;
    }

    // 从队列中取出数据并更新图表
    while (!adcDataQueue.isEmpty()) {
        if (adcDataQueue.isEmpty() || valueDataQueue.isEmpty()) {
            qDebug() << "Queue is empty, skipping iteration";
            continue;
        }

        auto adcData = adcDataQueue.dequeue();
        auto valueData = valueDataQueue.dequeue();

        // 检查 QPair 的内容是否有效
        if (adcData.second.isEmpty() || valueData.second.isEmpty()) {
            qDebug() << "Data is empty, skipping";
            continue;
        }

        uint32_t timestamp = adcData.first;
        const QVector<int16_t>& adcValues = adcData.second;
        const QVector<int16_t>& valueValues = valueData.second;

        for (uint8_t channel = 0; channel < total_sensor; ++channel) {
            for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; ++need_sensor_count) {
                int index = channel + need_sensor_count;
                if (index >= adcValues.size() || index >= valueValues.size()) {
                    qDebug() << "Index out of bounds, skipping index" << index;
                    continue;
                }

                int adc = adcValues[index];
                int value = valueValues[index];

                if (graph_adc_vector[channel + need_sensor_count] && graph_value_vector[channel + need_sensor_count]) {
                    graph_adc_vector[channel + need_sensor_count]->graph(0)->addData(timestamp, adc);
                    graph_value_vector[channel + need_sensor_count]->graph(0)->addData(timestamp, value);

                    // 更新图表范围
                    uint32_t x_min = 0;
                    if (counter >= 20000) {
                        x_min = counter - 20000;
                    }
                    graph_adc_vector[channel + need_sensor_count]->xAxis->setRange(x_min, counter);
                    graph_adc_vector[channel + need_sensor_count]->graph(0)->rescaleValueAxis();

                    graph_value_vector[channel + need_sensor_count]->xAxis->setRange(x_min, counter);
                    graph_value_vector[channel + need_sensor_count]->graph(0)->rescaleValueAxis();

                    // 更新 UI 文本
                    switch (sensor_v[channel]->para.f_module[need_sensor_count]) {
                    case MODULE_BTH:
                        ui->brush_adc->setText("电机adc：" + QString::number(adc));
                        ui->brush_value->setText("电机压力：" + QString::number(value));
                        graph_adc_vector[channel + need_sensor_count]->graph(0)->setName("电机ADC值");
                        graph_value_vector[channel + need_sensor_count]->graph(0)->setName("电机压力值");
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
                    default:
                        qDebug() << "Unknown module type:" << sensor_v[channel]->para.f_module[need_sensor_count];
                    }
                } else {
                    qDebug() << "Graph vector is null, skipping update for channel:" << channel + need_sensor_count;
                }
            }
        }

        // 处理帧率显示和倒计时
        if (transTime.isValid()) {
            send_frame_rate(QString("%1 ms").arg(transTime.elapsed() / adcValues.size()));
        } else {
            qDebug() << "transTime is not valid";
        }
        transTime.restart();

        if (ui->countdown) {
            ui->countdown->display((pressTestTime * 10 - countdowntime.elapsed()) / 1000);
        } else {
            qDebug() << "countdown widget is null";
        }
    }

    for (uint8_t channel = 0; channel < total_sensor; ++channel) {
        for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; ++need_sensor_count) {
            if (graph_value_vector[channel + need_sensor_count]) {
                graph_value_vector[channel + need_sensor_count]->replot();
            } else {
                qDebug() << "graph_value_vector is null, skipping replot for channel:" << channel + need_sensor_count;
            }
            if (graph_adc_vector[channel + need_sensor_count]) {
                graph_adc_vector[channel + need_sensor_count]->replot();
            } else {
                qDebug() << "graph_adc_vector is null, skipping replot for channel:" << channel + need_sensor_count;
            }
        }
    }
}
// void PressureSensorForm::updateGraphs() {
//     int need_sensor_data = total_sensor;
//     if (product_model == MODEL_ID_Y30S || product_model == MODEL_ID_Y20PS)
//         need_sensor_data = 2;
//     else
//         need_sensor_data = 1;
//     // 从队列中取出数据并更新图表
//     while (!adcDataQueue.isEmpty()) {
//         auto adcData = adcDataQueue.dequeue();
//         auto valueData = valueDataQueue.dequeue();

//         uint32_t timestamp = adcData.first;
//         const QVector<int16_t>& adcValues = adcData.second;
//         const QVector<int16_t>& valueValues = valueData.second;

//         for (uint8_t channel = 0; channel < total_sensor; channel++) {
//             for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; need_sensor_count++) {
//                 int index = channel + need_sensor_count;
//                 if (index >= adcValues.size() || index >= valueValues.size())
//                     continue;

//                 int adc = adcValues[index];
//                 int value = valueValues[index];

//                 graph_adc_vector[channel + need_sensor_count]->graph(0)->addData(timestamp, adc);
//                 graph_value_vector[channel + need_sensor_count]->graph(0)->addData(timestamp, value);

//                 // 更新图表范围
//                 uint32_t x_min = 0;
//                 if (counter >= 20000) {
//                     x_min = counter - 20000;
//                 }
//                 graph_adc_vector[channel + need_sensor_count]->xAxis->setRange(x_min, counter);
//                 graph_adc_vector[channel + need_sensor_count]->graph(0)->rescaleValueAxis();

//                 graph_value_vector[channel + need_sensor_count]->xAxis->setRange(x_min, counter);
//                 graph_value_vector[channel + need_sensor_count]->graph(0)->rescaleValueAxis();

//                 // 更新 UI 文本
//                 switch (sensor_v[channel]->para.f_module[need_sensor_count]) {
//                     case MODULE_BTH:
//                         ui->brush_adc->setText("电机adc：" + QString::number(adc));
//                         ui->brush_value->setText("电机压力：" + QString::number(value));
//                         graph_adc_vector[channel + need_sensor_count]->graph(0)->setName("电机ADC值");
//                         graph_value_vector[channel + need_sensor_count]->graph(0)->setName("电机压力值");
//                         break;
//                     case MODULE_MODE_BUTTON:
//                         ui->botton_adc->setText("模式按键adc：" + QString::number(adc));
//                         ui->botton_value1->setText("模式按键压力：" + QString::number(value));
//                         graph_adc_vector[channel + need_sensor_count]->graph(0)->setName("模式按键ADC值");
//                         graph_value_vector[channel + need_sensor_count]->graph(0)->setName("模式按键压力值");
//                         break;
//                     case MODULE_POWER_BUTTON:
//                         ui->power_adc->setText("电源按键adc：" + QString::number(adc));
//                         ui->power_value->setText("电源按键压力：" + QString::number(value));
//                         graph_adc_vector[channel + need_sensor_count]->graph(0)->setName("电源按键ADC值");
//                         graph_value_vector[channel + need_sensor_count]->graph(0)->setName("电源按键压力值");
//                         break;
//                     case MODULE_ASSISTANT_COMPONENT:
//                         ui->assistant_botton_adc->setText("辅助元件adc：" + QString::number(adc));
//                         ui->assistant_botton_value->setText("辅助元件压力：" + QString::number(value));
//                         graph_adc_vector[channel + need_sensor_count]->graph(0)->setName("辅助元件ADC值");
//                         graph_value_vector[channel + need_sensor_count]->graph(0)->setName("辅助元件压力值");
//                         break;
//                 }
//             }
//         }

//         // 处理帧率显示和倒计时
//         send_frame_rate(QString("%1 ms").arg(transTime.elapsed() / adcValues.size()));
//         transTime.restart();

//         ui->countdown->display((pressTestTime * 10 - countdowntime.elapsed()) / 1000);
//     }
//     for (uint8_t channel = 0; channel < total_sensor; channel++) {
//         for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; need_sensor_count++) {
//             graph_value_vector[channel + need_sensor_count]->replot();
//             graph_adc_vector[channel + need_sensor_count]->replot();
//         }
//     }
// }

void PressureSensorForm::graph_update(FacUploadPresSensor x) {
    if (graph_set_argu <= GRAPH_SET_CLOSE) {
        return;
    }

    int need_sensor_data = total_sensor;
    if (product_model == MODEL_ID_Y30S || product_model == MODEL_ID_Y20PS)
        need_sensor_data = 2;
    else
        need_sensor_data = 1;

    for (int i = 0; i < x.sensor_data_count; i++) {
        QVector<int16_t> adcValues;
        QVector<int16_t> valueValues;

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; need_sensor_count++) {
                switch (sensor_v[channel]->para.f_module[need_sensor_count]) {
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
                case MODULE_MAX:
                    break;
                }
            }
        }

        // 使用 counter 作为时间戳，将数据构造成 QPair 后入队
        adcDataQueue.enqueue(qMakePair(counter, adcValues));
        valueDataQueue.enqueue(qMakePair(counter, valueValues));

        counter += 10;
    }
    // 发射信号
    // emit dataReady();
    updateGraphs();
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
                    << "电机ADC"
                    << "电机压力"
                    << "模式按键ADC"
                    << "模式按键压力"
                    << "电源按键ADC"
                    << "电源按键压力";

            ProtocolPressSampleData protocolData;
            protocolData.timeStamp = static_cast<int>(x.sensor_data[i].timestamp);
            protocolData.adcValues << static_cast<int>(x.sensor_data[i].brush_head.adc)
                                   << static_cast<int>(x.sensor_data[i].mode_button.adc)
                                   << static_cast<int>(x.sensor_data[i].power_button.adc)
                                   << static_cast<int>(x.sensor_data[i].assistant_component.adc);
            protocolData.valueValues << static_cast<int>(x.sensor_data[i].brush_head.value)
                                     << static_cast<int>(x.sensor_data[i].mode_button.value)
                                     << static_cast<int>(x.sensor_data[i].power_button.value)
                                     << static_cast<int>(x.sensor_data[i].assistant_component.value);
            savePressDataToLocalFolder(protocolData, isfirstsavedata);

            isfirstsavedata = 0;
        }
    }
}

void PressureSensorForm::calib_send_result(void) {
    qDebug() << "发送校准系数";
    repeat_send_ok = 0;
    isStartSendCaliResult = 1;

    if (isStartSendCaliResult) { // 是否发送出去

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            switch (sensor_v[channel]->para.f_module[0]) {
            case MODULE_BTH:
                cali_result.calib_factor[MODULE_BTH] = sensor_v[channel]->calib_result[0];
                cali_result.temperature[MODULE_BTH] = sensor_v[channel]->temperature;
                showlog(QString("写入电机校准系数：") + QString::number(cali_result.calib_factor[MODULE_BTH]));
                showlog(QString("写入温度校准系数：") + QString::number(cali_result.temperature[MODULE_BTH]));
                qDebug() << "电机校准系数：" << cali_result.calib_factor[MODULE_BTH];
                break;
            case MODULE_MODE_BUTTON:
                cali_result.calib_factor[MODULE_MODE_BUTTON] = sensor_v[channel]->calib_result[0];
                cali_result.temperature[MODULE_MODE_BUTTON] = sensor_v[channel]->temperature;
                showlog(QString("写入模式按键校准系数：") +
                        QString::number(cali_result.calib_factor[MODULE_MODE_BUTTON]));
                if (sensor_v[channel]->temperature != 0) {
                    showlog(QString("写入温度校准系数：") +
                            QString::number(cali_result.temperature[MODULE_MODE_BUTTON]));
                }
                qDebug() << "模式按键校准系数：" << cali_result.calib_factor[MODULE_MODE_BUTTON];

                if (product_model == MODEL_ID_Y30S || product_model == MODEL_ID_Y20PS) {
                    cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT] = sensor_v[channel]->calib_result[1];
                    showlog(QString("写入辅助器件校准系数：") +
                            QString::number(cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT]));
                }

                break;
            case MODULE_POWER_BUTTON:
                cali_result.calib_factor[MODULE_POWER_BUTTON] = sensor_v[channel]->calib_result[0];
                showlog(QString("写入电源按键校准系数：") +
                        QString::number(cali_result.calib_factor[MODULE_POWER_BUTTON]));
                qDebug() << "电源按键校准系数：" << cali_result.calib_factor[MODULE_POWER_BUTTON];
                break;
            case MODULE_ASSISTANT_COMPONENT:
                cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT] = sensor_v[channel]->calib_result[0];
                showlog(QString("写入辅助器件校准系数：") +
                        QString::number(cali_result.calib_factor[MODULE_ASSISTANT_COMPONENT]));

                break;
            }
        }

        isStartSendCaliResult = 0;
        delay_msec(300);
        protocolManager.set(DeviceCmd::PressCaliResult, QVariant::fromValue(cali_result));
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
    short last_brush_adc = x.sensor_data[0].brush_head.adc;
    short last_botton_adc = x.sensor_data[0].mode_button.adc;
    for (int i = 0; i < x.sensor_data_count; i++) {
        int first_runing_suc_num = 0;

        int need_sensor_data = total_sensor;
        if (product_model == MODEL_ID_Y30S || product_model == MODEL_ID_Y20PS)
            need_sensor_data = 2;
        else
            need_sensor_data = 1;

        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; need_sensor_count++) {
                switch (sensor_v[channel]->para.f_module[need_sensor_count]) {
                case MODULE_BTH:
                    adc_c[channel + need_sensor_count] = x.sensor_data[i].brush_head.adc;
                    break;
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

        isStartcali = 1; // 开始校准
        if (first_start_test) {
            for (int i = 0; i < total_sensor; i++) {
                // qDebug() << "i" << i << "gs32SensorFlag" << sensor_v[i]->gs32SensorFlag;
                if (sensor_v[i]->gs32SensorFlag < 2) {
                    sensor_v[i]->ndt_sensor_cali_process(x.sensor_data_count, adc_c + i); // 需要跑一hui
                } else if (sensor_v[i]->gs32SensorFlag == 2) {
                    first_runing_suc_num++;
                }
            }
            if (first_runing_suc_num == total_sensor) { //要所有通道都OK
                first_start_test = 0;
                for (int i = 0; i < total_sensor; i++) {
                    sensor_v[i]->para.first_adc = adc_c[i];
                    showlog("记录初始adc值" + QString::number(sensor_v[i]->para.first_adc) + "通道" +
                            QString::number(i));
                }
            }
            continue;
        }
        // 避免电机给按键校准造成干扰需要检查 brush_head.value < 50，即电机值较小时才进行校准，避免干扰
        if (sensor_v[calib_chan]->para.f_module[0] == MODULE_MODE_BUTTON) {
            if ((short)x.sensor_data[i].brush_head.value < 50)
                if (1) {
                    if ((adc_c[calib_chan] - sensor_v[calib_chan]->para.first_adc) >
                            sensor_v[calib_chan]->para.adc_threshold &&
                        !start_calib_channel[calib_chan] && sensor_v[calib_chan]->para.first_adc != 0 &&
                        qAbs(adc_c[calib_chan] - last_botton_adc) < AdcShakeValue

                    ) {
                        brush_before_calib_count++;
                        qDebug() << getIndex() << "按键合法值个数" << brush_before_calib_count << "要求个数"
                                 << sensor_v[calib_chan]->para.count_threshold;
                        if (brush_before_calib_count > (sensor_v[calib_chan]->para.count_threshold - 200)) {
                            start_calib_channel[calib_chan] = 1;
                            brush_before_calib_count = 0;
                        }
                    } else {
                        // qDebug() << getIndex()
                        //          << "ADC 差值: " << adc_c[calib_chan] - sensor_v[calib_chan]->para.first_adc
                        //          << " ADC 阈值: " << sensor_v[calib_chan]->para.adc_threshold
                        //          << " 校准是否已开始: " << start_calib_channel[calib_chan]
                        //          << " 校准通道: " << calib_chan
                        //          << " 初始 ADC 值: " << sensor_v[calib_chan]->para.first_adc
                        //          << " adc_c[calib_chan]: " << (adc_c[calib_chan])
                        //          << " last_botton_adc: " << (last_botton_adc)
                        //          << " 稳定差值: " << qAbs(adc_c[calib_chan] - last_botton_adc);
                    }
                }
        } else {
            // qDebug() << "ADC 差值: " << adc_c[calib_chan] - sensor_v[calib_chan]->para.first_adc
            //          << " ADC 阈值: " << sensor_v[calib_chan]->para.adc_threshold
            //          << " 校准是否已开始: " << start_calib_channel[calib_chan] << " 校准通道: " << calib_chan
            //          << " 初始 ADC 值: " << sensor_v[calib_chan]->para.first_adc
            //          << " adc_c[calib_chan]: " << (adc_c[calib_chan]) << " last_brush_adc: " << (last_brush_adc)
            //          << " 稳定差值: " << qAbs(adc_c[calib_chan] - last_brush_adc);

            if ((adc_c[calib_chan] - sensor_v[calib_chan]->para.first_adc) > sensor_v[calib_chan]->para.adc_threshold &&
                !start_calib_channel[calib_chan] && sensor_v[calib_chan]->para.first_adc != 0 &&
                qAbs(adc_c[calib_chan] - last_brush_adc) < AdcShakeValue) {
                brush_before_calib_count++;
                qDebug() << getIndex() << "电机合法值个数" << brush_before_calib_count << "要求个数"
                         << sensor_v[calib_chan]->para.count_threshold;
                if (brush_before_calib_count > sensor_v[calib_chan]->para.count_threshold) {
                    start_calib_channel[calib_chan] = 1;
                    brush_before_calib_count = 0;
                }
            }
        }

        if (sensor_v[calib_chan]->gs32SensorFlag == 1) {
            // sensor_v[calib_chan]->ndt_sensor_cali_process(x.sensor_data_count, adc_c + calib_chan);
        }

        // qDebug() << "Condition met: gs32SensorFlag == 2" << sensor_v[calib_chan]->gs32SensorFlag
        //          << "start_calib_channel == " << start_calib_channel[calib_chan]
        //          << ", calib_status == " << sensor_v[calib_chan]->calib_status;
        if (sensor_v[calib_chan]->gs32SensorFlag == 2 && start_calib_channel[calib_chan] &&
            sensor_v[calib_chan]->calib_status != CALIB_NOCALIB) {
            sensor_v[calib_chan]->ndt_sensor_cali_process(x.sensor_data_count, adc_c + calib_chan);
        }

        if (sensor_v[calib_chan]->gs32SensorFlag == 3) {
            unsigned short* cali_ok =
                sensor_v[calib_chan]->ndt_sensor_cali_process(x.sensor_data_count, adc_c + calib_chan);

            sensor_v[calib_chan]->calib_status = CALIB_SUC;
            // qDebug() << "cali_ok[0] " << cali_ok[0];

            // qDebug() << "calib_chan " << calib_chan;
            // qDebug() << " sensor_v[calib_chan]calib_status" << sensor_v[calib_chan]->calib_status;
            //  qDebug() << " sensor_v[calib_chan]->calib_result[1]" << sensor_v[calib_chan]->calib_result[1];
            // qDebug() << " sensor_v[calib_chan]->calib_result[0]" << sensor_v[calib_chan]->calib_result[0];

            if (cali_ok[0] != 0) {
                sensor_v[calib_chan]->calib_result = cali_ok;

                sensor_v[calib_chan]->temperature = x.sensor_data[i].temperature;
                sensor_v[calib_chan]->sensor_cali_set(0);
            }
            if (calib_chan + 1 == total_sensor) {
                isCaliOk = 1; // 校准完成的标志
            }
        }

        // 在函数内使用标志，确保每个条件只运行一次
        if (sensor_v[calib_chan]->gs32SensorFlag == 1 && !donotmoveFlag) {
            // showlog(donotmove);
            donotmoveFlag = true;
        }

        if (sensor_v[calib_chan]->gs32SensorFlag == 2 && !brushPlacingFlag && is_get_ready_command) {
            brushPlacingFlag = true;
        }

        if (sensor_v[calib_chan]->gs32SensorFlag == 3 && !packWeightsFlag) {
            packWeightsFlag = true;
        }

        if (sensor_v[calib_chan]->gs32SensorFlag == 4 && !bottonPlacingFlag) {
            showlog(botton_placing_200_weights);
            bottonPlacingFlag = true;
        }

        if (sensor_v[calib_chan]->gs32SensorFlag == 5 && !caliResultOkFlag) {
            showlog(cali_result_ok);
            caliResultOkFlag = true;
        }

        if (sensor_v[calib_chan]->gs32SensorFlag == 6 && !caliResultFailFlag) {
            showlog(cali_result_fail);
            caliResultFailFlag = true;

            if (product_model == MODEL_ID_Y30S || product_model == MODEL_ID_Y20PS) {
                showlog(sensor_v[calib_chan]->ui_msg_err[0] + QString::number(sensor_v[calib_chan]->err[0]));
                showlog(sensor_v[calib_chan]->ui_msg_err[0] + QString::number(sensor_v[calib_chan]->err[1]));
            } else {
                showlog(sensor_v[calib_chan]->ui_msg_err[0] + QString::number(sensor_v[calib_chan]->err[0]));
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
    QColor backgroundColor = QColor("#00FF00"); // green
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

    default:
        backgroundColor = QColor("#00FF00");
        break;
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
    if (product_model == MODEL_ID_Y30S || product_model == MODEL_ID_Y20PS)
        need_sensor_data = 2;
    else
        need_sensor_data = 1;

    for (int i = 0; i < x.sensor_data_count; i++) {
        for (uint8_t channel = 0; channel < total_sensor; channel++) {
            for (uint8_t need_sensor_count = 0; need_sensor_count < need_sensor_data; need_sensor_count++) {
                switch (sensor_v[channel]->para.f_module[need_sensor_count]) {
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

        switch (sensor_v[test_chan]->para.f_module[0]) {
        case MODULE_BTH:
            if (sensor_v[test_chan]->test_status == TEST_START) {
                static uint8_t lock = 0;
                if (pb->getState(Qpb::PbStateType::DevIntoWhiteMode) ||
                    pb->getState(Qpb::PbStateType::MotorTestState)) {
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                    if (product_model == MODEL_ID_F20) {
                        showlog("当前是电机测试");
                    } else {
                        protocolManager.set(DeviceCmd::BrushControl, 1);
                    }

                    showlog(sensor_v[test_chan]->ui_msg_test[0]);
                    appendFormattedText(ui->tip, sensor_v[test_chan]->ui_msg_test[0], QColor("black"));

                    if (product_model == MODEL_ID_Y20) {
                        sensor_v[test_chan]->test_status = TEST_BTH_NORMAL;
                        showlog("当前是y20的测试");
                        // showlog("放上350g砝码");
                        countdowntime.start();
                    } else {
                        showlog("当前是正常测试");
                        sensor_v[test_chan]->test_status = TEST_BTH_SHAKE;
                        set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                    }

                } else if (lock == 0) {
                    lock = 1;
                    if (product_model == MODEL_ID_F20) {
                        protocolManager.set(DeviceCmd::MotorTestState, 1);
                    } else {
                        protocolManager.set(DeviceCmd::DeviceMode, 3); // 清洁模式
                    }
                    showlog("开始压感测试");
                    delay_msec(200);
                    lock = 0;
                }
            }
            // else if (sensor_v[test_chan]->test_status == TEST_BTH_END) {
            //     sensor_v[test_chan]->test_status = TEST_BTH_OVERPRESS;
            // }
            break;

        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
#if 0 // USE_KEY_CLICK_TEST // 按键测试 0:使用按键是否响应的方案 1：使用200g砝码值比对的方案
                if (sensor_v[test_chan]->test_status == TEST_START) {
                    sensor_v[test_chan]->test_status = TEST_KEY_NORMAL;
                    showlog(sensor_v[test_chan]->ui_msg_test[0]);
                    qDebug() << "TEST_KEY_NORMAL";
                }
#else
            if (sensor_v[test_chan]->test_status == TEST_START) {
                qDebug() << "请求获取按键状态上报" << i;
                protocolManager.get(DeviceCmd::ButtonState, 1);
                sensor_v[test_chan]->test_status = TEST_KEY_NO_CLICK;
                qDebug() << "set_fixture_movement:key_test";
                showlog(sensor_v[test_chan]->ui_msg_test[0]);
                set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                qDebug() << "TEST_KEY_NO_CLICK";
            }
#endif
            break;
        }

        if (sensor_v[test_chan]->test_status == TEST_BTH_SHAKE) {
            if (value_c[test_chan] > 50 - 20 && value_c[test_chan] < 50 + 20) // 数据准确
            {
                qDebug("TEST_BTH_SHAKE:%d", sensor_v[test_chan]->para.current_count);
                sensor_v[test_chan]->para.current_count++;
                if (sensor_v[test_chan]->para.current_count >= 200) {
                    sensor_v[test_chan]->para.current_count = 0;
                    sensor_v[test_chan]->test_status = TEST_BTH_NORMAL;
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                    appendFormattedText(ui->tip, sensor_v[test_chan]->ui_msg_test[1], QColor("black"));
                    showlog(sensor_v[test_chan]->ui_msg_test[1]);
                }
            }
            sensor_v[test_chan]->test_result[0] = 50;
        } else if (sensor_v[test_chan]->test_status == TEST_BTH_NORMAL) {
            if (value_c[test_chan] > 250 - 0.2 * 250 && value_c[test_chan] < 250 + 0.2 * 250) // 数据准确
            {
                qDebug("TEST_BTH_NORMAL:%d", sensor_v[test_chan]->para.current_count);
                sensor_v[test_chan]->para.current_count++;

                if (sensor_v[test_chan]->para.current_count >= 200) {
                    sensor_v[test_chan]->para.current_count = 0;
                    sensor_v[test_chan]->test_status = TEST_BTH_OVERPRESS;
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                    appendFormattedText(ui->tip, sensor_v[test_chan]->ui_msg_test[2], QColor("black"));
                    showlog(sensor_v[test_chan]->ui_msg_test[2]);
                }
            } else if (value_c[test_chan] > 400) { // 不该过压的时候过压
                sensor_v[test_chan]->para.error_count++;
                if (sensor_v[test_chan]->para.error_count >= 80) {
                    sensor_v[test_chan]->para.error_count = 0;
                    sensor_v[test_chan]->test_status = TEST_FAIL;
                }
            }
            sensor_v[test_chan]->test_result[0] = 350;

            if (product_model == MODEL_ID_Y20) {
                if (value_c[test_chan] < 5) // 未过压
                {
                    qDebug("TEST_BTH_NORMAL:%d", sensor_v[test_chan]->para.current_count);
                    sensor_v[test_chan]->para.current_count++;
                    if (sensor_v[test_chan]->para.current_count >= pressTestTime) {
                        sensor_v[test_chan]->para.current_count = 0;
                        sensor_v[test_chan]->test_status = TEST_BTH_OVERPRESS;
                        showlog(sensor_v[test_chan]->ui_msg_test[2]);

                        appendFormattedText(ui->tip, sensor_v[test_chan]->ui_msg_test[2], QColor("black"));
                        countdowntime.restart();
                    }
                } else if (value_c[test_chan] >= 5) {
                    sensor_v[test_chan]->para.current_count = 0;
                    sensor_v[test_chan]->test_status = TEST_FAIL;
                }
            }

        } else if (sensor_v[test_chan]->test_status == TEST_BTH_OVERPRESS) {
            if (value_c[test_chan] > 450 - 0.2 * 450 && value_c[test_chan] < 450 + 0.2 * 450) // 数据准确
            {
                qDebug("TEST_BTH_OVERPRESS:%d", sensor_v[test_chan]->para.current_count);
                sensor_v[test_chan]->para.current_count++;
                if (sensor_v[test_chan]->para.current_count >= 300) {
                    sensor_v[test_chan]->para.current_count = 0;
                    sensor_v[test_chan]->test_status = TEST_SUC;
                }
            }
            sensor_v[test_chan]->test_result[0] = 450;
            if (product_model == MODEL_ID_Y20) {
                if (value_c[test_chan] >= 5) // 过压
                {
                    qDebug("TEST_BTH_OVERPRESS:%d", sensor_v[test_chan]->para.current_count);
                    sensor_v[test_chan]->para.current_count++;
                    if (sensor_v[test_chan]->para.current_count >= 200) {
                        sensor_v[test_chan]->para.current_count = 0;
                        sensor_v[test_chan]->test_status = TEST_SUC;
                    }
                }
            }

        } else if (sensor_v[test_chan]->test_status == TEST_KEY_NORMAL) {
            qDebug("TEST_KEY_NORMAL:%d", sensor_v[test_chan]->para.current_count);
            if (value_c[test_chan] > 200 - 30 && value_c[test_chan] < 200 + 30) {
                sensor_v[test_chan]->para.current_count++;
                if (sensor_v[test_chan]->para.current_count >= 200) {
                    sensor_v[test_chan]->test_result[0] = value_c[test_chan];
                    sensor_v[test_chan]->para.current_count = 0;
                    sensor_v[test_chan]->test_status = TEST_SUC;
                }
            }
        } else if (sensor_v[test_chan]->test_status == TEST_KEY_NO_CLICK) {
            qDebug("TEST_KEY_NO_CLICK:%d", sensor_v[test_chan]->para.current_count);

#if 0 // 打开就无视电容触发
            if (sensor_v[test_chan]->para.no_click_max < value_c[test_chan]) {
                sensor_v[test_chan]->para.no_click_max = value_c[test_chan];
            }
            if (sensor_v[test_chan]->para.no_click_max > 200) {
                sensor_v[test_chan]->para.button_state = 1;
            }
#endif
            if (sensor_v[test_chan]->send_state == 1) {
                sensor_v[test_chan]->send_state = 0; //复位治具状态
                if (sensor_v[test_chan]->para.button_state == 0) {
                    // sensor_v[test_chan]->test_result[0] = 100;
                    sensor_v[test_chan]->para.button_state = 0;
                    sensor_v[test_chan]->test_status = TEST_KEY_CLICK;
                    showlog(sensor_v[test_chan]->ui_msg_test[1]);
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                } else if (sensor_v[test_chan]->para.button_state == 1) {
                    sensor_v[test_chan]->test_status = TEST_FAIL;
                    showlog("测试失败：不需要触发，按键触发");
                }
            }
        } else if (sensor_v[test_chan]->test_status == TEST_KEY_CLICK) {
            qDebug("TEST_KEY_CLICK:%d", sensor_v[test_chan]->para.current_count);
// 无视电容触发
#if 0
            if (sensor_v[test_chan]->para.click_max < value_c[test_chan]) {
                sensor_v[test_chan]->para.click_max = value_c[test_chan];
            }
            if (sensor_v[test_chan]->para.click_max > 250) {
                sensor_v[test_chan]->para.button_state = 1;
            }
#endif
            if (sensor_v[test_chan]->send_state == 1) {
                sensor_v[test_chan]->send_state = 0;
                if (sensor_v[test_chan]->para.button_state == 1) {
                    // sensor_v[test_chan]->test_result[0] = 0;
                    sensor_v[test_chan]->para.button_state = 0;
                    sensor_v[test_chan]->test_status = TEST_SUC;
                    set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                    showlog("按键测试成功：成功触发");
                } else if (sensor_v[test_chan]->para.button_state == 0) {
                    sensor_v[test_chan]->test_status = TEST_FAIL;
                    showlog("测试失败：需要触发，按键未触发");
                }
            }
        }
    }
}

void PressureSensorForm::refreshPressSensorData(ProtocolPressSampleData x) {
    if (x.adcValues.size() >= 4 && x.valueValues.size() >= 4) {
        ui->brush_adc->setText("电机adc：" + QString::number(x.adcValues[0]));
        ui->brush_value->setText("电机压力：" + QString::number(x.valueValues[0]));
        ui->botton_adc->setText("模式按键adc：" + QString::number(x.adcValues[1]));
        ui->botton_value1->setText("模式按键压力：" + QString::number(x.valueValues[1]));
        ui->power_adc->setText("电源按键adc：" + QString::number(x.adcValues[2]));
        ui->power_value->setText("电源按键压力：" + QString::number(x.valueValues[2]));
        ui->assistant_botton_adc->setText("辅助元件adc：" + QString::number(x.adcValues[3]));
        ui->assistant_botton_value->setText("辅助元件压力：" + QString::number(x.valueValues[3]));
    }
    savePressDataToLocalFolder(x, isfirstsavedata);
    isfirstsavedata = 0;
}

void PressureSensorForm::refreshButton(ProtocolButtonStateData x) {
    showlog("获取到按键上报");
    qDebug() << "product_model=" << product_model;
    qDebug() << "nsor_v[0].para.f_module[0]" << sensor_v[0]->para.f_module[0];

    if (sensor_v[0]->para.f_module[0] == MODULE_MODE_BUTTON && (product_model != MODEL_ID_U7) &&
        (product_model != MODEL_ID_F20) && (product_model != MODEL_ID_Y30S)) {
        showlog("模式状态" + QString::number(x.modeButtonState));
        if (x.modeButtonState == ButtonState_PRESSED) {
            sensor_v[test_chan]->para.button_state = 1;
        }
    }
    if (sensor_v[0]->para.f_module[0] == MODULE_POWER_BUTTON ||
        (product_model == MODEL_ID_F20 || product_model == MODEL_ID_U7) || (product_model == MODEL_ID_Y30S)) {
        showlog("电源状态" + QString::number(x.powerButtonState));

        if (x.powerButtonState == ButtonState_PRESSED) {
            sensor_v[test_chan]->para.button_state = 1;
        }
    }
}

void PressureSensorForm::delay_msec(unsigned int msec) {
    // QEventLoop loop;                                // 定义一个新的事件循环
    // QTimer::singleShot(msec, &loop, SLOT(quit()));  // 创建单次定时器，槽函数为事件循环的退出函数
    // loop.exec();  // 事件循环开始执行，程序会卡在这里，直到定时时间到，本循环被退出
    waitWork(msec);
}

void PressureSensorForm::refreshSn(ProtocolSnData data) {
    stringsn = data.value;
    qDebug() << "dev_info" << data.value;
    qDebug() << "stringsn" << stringsn;
    showlog("芯片存储的整机sn:" + stringsn);
}

void PressureSensorForm::refreshBleState(int state) {
    if (state) {
        ui->bleStatusLabel->setText("蓝牙连接：<font color='green'>成功</font>");
        //   showlog("蓝牙连接成功");
        protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
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
    isCaliOk = 0;              // 是否校准完成
    isStartSendCaliResult = 0; // 是否开始发送校验结果
    isovertime = 0;            // 超时标志位

    data_200_true_count = 0;
    data_250_true_count = 0;
    data_under_pressure_error_count = 0;
    data_350_true_count = 0;
    data_over_pressure_error_count = 0;
    Amplitudetimes = 0;
    Amplituderesult = 0;
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
    first_start_test = 1; // 只进行一次
    isfirstsavedata = 1;  // 是否第一次保存压感数据
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
    memset(&start_calib_channel, 0, sizeof(start_calib_channel)); // 是否开始电机200校准

    // emit operator_instruct(getIndex());  //核对

    set_independent_state(STATE_INVALID);
    waitWork(1000); //给开机时间
    at->set(DongleCmd::BleScanConnect, ui->macInput->text());
    showlog("开始测试");
}

void PressureSensorForm::set_fixture_movement(MODEL_ID_E model, State state, int argument) {
    if (sensor_v.empty()) {
        showlog("模组未初始化,取消发送治具指令");
        return;
    }

    qDebug() << "设置治具参数" << model << state << argument;
    switch (model) {
    case MODEL_ID_Y20P:
        Y20P_fixture(state, argument);
        break;
    case MODEL_ID_Y30P:
        Y20P_fixture(state, argument);
        break;

    case MODEL_ID_Y20:
        Y20P_fixture(state, argument);
        break;
    case MODEL_ID_F20:
        F20_fixture(state, argument);
        break;
    case MODEL_ID_U7:
        U7_fixture(state, argument);
        break;
    case MODEL_ID_P30P:
        // 治具指令跟U7一致
        U7_fixture(state, argument);
        break;
    case MODEL_ID_Y30S:
        // 治具指令跟U7一致
        U7_fixture(state, argument);
        break;
    case MODEL_ID_Y20PS:

        // 治具指令跟y20p一致
        // if (getIndex() == 1)
        Y20PS_fixture(state, argument);
        break;
    default:
        showlog("无治具配置");
        break;
    }
}

void PressureSensorForm::Y20P_fixture(State state, int argument) {
    switch (state) {
    case STATE_CALIB_CH_X:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            set_independent_state(STATE_CALIB_CH0);
            send_start_command(COMMAND_ID_BTH_DOWN_200, 0);
            break;

        case MODULE_MODE_BUTTON:
            send_start_command(COMMAND_ID_KEY_DOWN_200, 0);
            break;
        }
        break;

    case STATE_CALIB_CH_X_RESULT:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:

            set_independent_state(STATE_CALIB_CH1);
            send_start_command(COMMAND_ID_BTH_UP, 0);

            break;

        case MODULE_MODE_BUTTON:
            send_start_command(COMMAND_ID_KEY_UP, 0);
            break;
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
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            switch (sensor_v[test_chan]->test_status) {
            case TEST_BTH_SHAKE:
                send_start_command(COMMAND_ID_BTH_PRESS_50, 0);
                break;

            case TEST_BTH_NORMAL:
                send_start_command(COMMAND_ID_BTH_PRESS_350, 0);
                break;

            case TEST_BTH_OVERPRESS:
                send_start_command(COMMAND_ID_BTH_PRESS_450, 0);
                break;
            }
            break;
        }
        break;

    case STATE_TEST_CH_X_RESULT:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            send_start_command(COMMAND_ID_BTH_PRESS_UP, 0);
            break;
        }
        break;
    }
}

void PressureSensorForm::F20_fixture(State state, int argument) {
    switch (state) {
    case STATE_CALIB_CH_X:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            send_start_command(COMMAND_ID_FAMA_400_O, 0);
            break;
        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
            send_start_command(COMMAND_ID_FAMA_300_O, 0);
            break;
        }
        break;
    case STATE_CALIB_CH_X_RESULT:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            send_start_command(COMMAND_ID_FAMA_400_C, 0);
            break;
        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
            send_start_command(COMMAND_ID_FAMA_300_C, 0);
            break;
        }
        break;

    case STATE_TEST_CH_X:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            switch (sensor_v[test_chan]->test_status) {
            case TEST_BTH_SHAKE:
                send_start_command(COMMAND_ID_FAMA_50_O, 0);
                break;

            case TEST_BTH_NORMAL:
                send_start_command(COMMAND_ID_FAMA_200_O, 0);
                break;

            case TEST_BTH_OVERPRESS:
                send_start_command(COMMAND_ID_FAMA_400_O, 0);
                break;
            }
            break;
        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
            switch (sensor_v[test_chan]->test_status) {
            case TEST_KEY_NO_CLICK:
                send_start_command(COMMAND_ID_FAMA_100_O, 0);
                delay_msec(500);
                send_start_command(COMMAND_ID_FAMA_100_C, 0);
                delay_msec(500);
                sensor_v[test_chan]->send_state = 1;
                break;

            case TEST_KEY_CLICK:
                send_start_command(COMMAND_ID_FAMA_300_O, 0);
                delay_msec(500);
                send_start_command(COMMAND_ID_FAMA_300_C, 0);
                delay_msec(1000);
                sensor_v[test_chan]->send_state = 1;
                break;

            case MODULE_BTH:
                set_independent_state(STATE_CALIB_CH0);
                break;
            }
            break;
        }
        break;

    case STATE_TEST_CH_X_RESULT:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            send_start_command(COMMAND_ID_FAMA_400_C, 0);
            break;
        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
            send_start_command(COMMAND_ID_FAMA_300_C, 0);
            break;
        }

        break;

    case STATE_SAVE_RESULT:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            send_start_command(COMMAND_ID_FAMA_400_C, 0);
            delay_msec(1000);
            send_start_command(COMMAND_ID_FAMA_UP, 0);
            break;
        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
            send_start_command(COMMAND_ID_FAMA_300_C, 0);
            break;
        }
        break;
    default:
        break;
    }
}
void PressureSensorForm::Y20PS_fixture(State state, int argument) {
    if (sensor_v.empty()) {
        showlog("模组未初始化,取消发送治具指令");
        return;
    }
    switch (state) {
    case STATE_CALIB_CH_X:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            set_independent_state(STATE_CALIB_CH0);
            send_start_command(COMMAND_ID_BTH_DOWN_200, 0);
            break;

        case MODULE_MODE_BUTTON:
            set_independent_state(STATE_CALIB_CH1);
            send_start_command(COMMAND_ID_KEY_DOWN_200, 0);
            break;
        }
        break;

    case STATE_CALIB_CH_X_RESULT:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:

            set_independent_state(STATE_CALIB_CH1);
            send_start_command(COMMAND_ID_BTH_UP, 0);
            break;

        case MODULE_MODE_BUTTON:
            send_start_command(COMMAND_ID_KEY_UP, 0);
            break;
        }

        if (calib_chan + 1 == total_sensor) {
            send_start_command(COMMAND_ID_FIXED_BLOCK_UP, 0);
            if (SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() == 2)
                waitWork(3000);
            send_start_command(COMMAND_ID_TRAY_OUT, 0);
        }
        break;

    case STATE_WATI_ASKQR:

        send_start_command(COMMAND_ID_TRAY_IN, 0);
        if (SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() == 2)
            waitWork(3000);
        // waitWork(2500);
        // send_start_command(COMMAND_ID_FIXED_BLOCK_DOWN, 0);
        // waitWork(1500);

        break;

    case STATE_SAVE_RESULT:
        set_independent_state(STATE_CALIB_RESULT);

        if (SETTINGS.value("PRESSURE/PressMechine").toInt() == 1) {
            qDebug() << "治具结束";
            //没有电机去掉了
            if (SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() != 2) //测试不需要
                send_start_command(COMMAND_ID_KEY_UP, 0);                  // FAMA_U

            if (SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() != 2) //测试不需要
                send_start_command(COMMAND_ID_FIXED_BLOCK_UP, 0);

            if (SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() == 2) //测试需要
                waitWork(3000);
            send_start_command(COMMAND_ID_TRAY_OUT, 0);
        }

        if (SETTINGS.value("PRESSURE/PressMechine").toInt() == 2) {
            send_start_command(COMMAND_ID_BTH_PRESS_UP, 0);
            send_start_command(COMMAND_ID_RESULT, 0);
        }

        break;

    case STATE_TEST_CH_X:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            switch (sensor_v[test_chan]->test_status) {
            case TEST_BTH_SHAKE:
                send_start_command(COMMAND_ID_BTH_PRESS_50, 0);
                break;

            case TEST_BTH_NORMAL:
                send_start_command(COMMAND_ID_BTH_PRESS_350, 0);
                break;

            case TEST_BTH_OVERPRESS:
                send_start_command(COMMAND_ID_BTH_PRESS_450, 0);
                break;
            }

        case MODULE_MODE_BUTTON:
            switch (sensor_v[test_chan]->test_status) {
            case TEST_KEY_NO_CLICK:
                send_start_command(COMMAND_ID_FAMA_100_O, 0);
                if (SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() == 2)
                    delay_msec(3000);

                sensor_v[test_chan]->send_state = 1;
                break;

            case TEST_KEY_CLICK:

                send_start_command(COMMAND_ID_FAMA_300_O, 0);
                if (SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() == 2)
                    delay_msec(3000);
                send_start_command(COMMAND_ID_FAMA_300_C, 0);
                if (SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() == 2)
                    delay_msec(3000);
                send_start_command(COMMAND_ID_FAMA_100_C, 0);
                if (SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() == 2)
                    delay_msec(6000);
                sensor_v[test_chan]->send_state = 1;
                break;
            }
            break;
        }
        break;

    case STATE_TEST_CH_X_RESULT:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_BTH:
            send_start_command(COMMAND_ID_BTH_PRESS_UP, 0);
            break;
        }
        break;
    }

    // switch (state) {
    // case STATE_CALIB_CH_X:
    //     switch (sensor_v[argument]->para.f_module[0]) {
    //     case MODULE_MODE_BUTTON:
    //     case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_O, 0); break;
    //     }
    //     break;

    // case STATE_CALIB_CH_X_RESULT:
    //     switch (sensor_v[argument]->para.f_module[0]) {
    //     case MODULE_MODE_BUTTON:
    //     case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
    //     }
    //     break;

    // case STATE_TEST_CH_X:
    //     switch (sensor_v[argument]->para.f_module[0]) {
    //     case MODULE_MODE_BUTTON:
    //     case MODULE_POWER_BUTTON:
    //         switch (sensor_v[test_chan]->test_status) {
    //         case TEST_KEY_NO_CLICK:
    //             send_start_command(COMMAND_ID_FAMA_100_O, 0);
    //             delay_msec(500);
    //             send_start_command(COMMAND_ID_FAMA_100_C, 0);
    //             delay_msec(500);
    //             sensor_v[test_chan]->send_state = 1;
    //             break;

    //         case TEST_KEY_CLICK:
    //             send_start_command(COMMAND_ID_FAMA_300_O, 0);
    //             delay_msec(500);
    //             send_start_command(COMMAND_ID_FAMA_300_C, 0);
    //             delay_msec(500);
    //             sensor_v[test_chan]->send_state = 1;
    //             break;
    //         }
    //         break;
    //     }
    //     break;

    // case STATE_TEST_CH_X_RESULT:
    //     switch (sensor_v[argument]->para.f_module[0]) {
    //     case MODULE_MODE_BUTTON:
    //     case MODULE_POWER_BUTTON: send_start_command(COMMAND_ID_FAMA_300_C, 0); break;
    //     }
    //     break;

    // case STATE_SAVE_RESULT:
    //     switch (sensor_v[argument]->para.f_module[0]) {
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
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
            send_start_command(COMMAND_ID_FAMA_300_O, 0);
            break;
        }
        break;

    case STATE_CALIB_CH_X_RESULT:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
            send_start_command(COMMAND_ID_FAMA_300_C, 0);
            break;
        }
        break;

    case STATE_TEST_CH_X:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
            switch (sensor_v[test_chan]->test_status) {
            case TEST_KEY_NO_CLICK:
                send_start_command(COMMAND_ID_FAMA_100_O, 0);
                delay_msec(500);
                send_start_command(COMMAND_ID_FAMA_100_C, 0);
                delay_msec(500);
                sensor_v[test_chan]->send_state = 1;
                break;

            case TEST_KEY_CLICK:
                send_start_command(COMMAND_ID_FAMA_300_O, 0);
                delay_msec(500);
                send_start_command(COMMAND_ID_FAMA_300_C, 0);
                delay_msec(1000);
                sensor_v[test_chan]->send_state = 1;
                break;
            }
            break;
        }
        break;

    case STATE_TEST_CH_X_RESULT:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
            send_start_command(COMMAND_ID_FAMA_300_C, 0);
            break;
        }
        break;

    case STATE_SAVE_RESULT:
        switch (sensor_v[argument]->para.f_module[0]) {
        case MODULE_MODE_BUTTON:
        case MODULE_POWER_BUTTON:
            send_start_command(COMMAND_ID_FAMA_300_C, 0);
            break;
        }
        break;

    default:
        break;
    }
}

void PressureSensorForm::ui_msg_show(MODEL_ID_E model, State state, int argument) {
    qDebug() << "ui_msg_show参数" << model << state << argument;
    switch (model) {
    case MODEL_ID_Y30S:
        if (state == STATE_CALIB_CH_X) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[1]);
        } else if (state == STATE_CALIB_CH_X_RESULT) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[2]);
        }

        break;
    case MODEL_ID_Y20PS:
        if (state == STATE_CALIB_CH_X) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[1]);
        } else if (state == STATE_CALIB_CH_X_RESULT) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[2]);
        }

        break;

    case MODEL_ID_Y20P:
        if (state == STATE_CALIB_CH_X) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[1]);
        } else if (state == STATE_CALIB_CH_X_RESULT) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[2]);
        }

        break;

    case MODEL_ID_Y20:
        if (state == STATE_CALIB_CH_X) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[1]);
        } else if (state == STATE_CALIB_CH_X_RESULT) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[2]);
        }

        break;

    case MODEL_ID_F20:
        if (state == STATE_CALIB_CH_X) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[1]);
        } else if (state == STATE_CALIB_CH_X_RESULT) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[2]);
        }
        break;
    case MODEL_ID_Y30P:
        if (state == STATE_CALIB_CH_X) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[1]);
        } else if (state == STATE_CALIB_CH_X_RESULT) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[2]);
        }
        break;

    case MODEL_ID_U7:
        if (state == STATE_CALIB_CH_X) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[1]);
        } else if (state == STATE_CALIB_CH_X_RESULT) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[2]);
        } else if (state == STATE_TEST_CH_X) {
            showlog(QString("测试通道%1:按键").arg(test_chan));
        } else if (state == STATE_TEST_CH_X_RESULT) {
            showlog(QString("通道%1：按键已测试完毕").arg(test_chan));
        } else if (state == STATE_CALIB_ALL_RESULT) {
            showlog("测试结束");
        }
        break;

    case MODEL_ID_Y21:
        if (state == STATE_CALIB_CH_X) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[1]);
        } else if (state == STATE_CALIB_CH_X_RESULT) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[2]);
        } else if (state == STATE_TEST_CH_X) {
            showlog(QString("测试通道%1:按键").arg(test_chan));
        } else if (state == STATE_TEST_CH_X_RESULT) {
            showlog(QString("通道%1：按键已测试完毕").arg(test_chan));
        } else if (state == STATE_CALIB_ALL_RESULT) {
            showlog("测试结束");
        }
        break;

    case MODEL_ID_P30P:
        if (state == STATE_CALIB_CH_X) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[1]);
        } else if (state == STATE_CALIB_CH_X_RESULT) {
            showlog(sensor_v[calib_chan]->ui_msg_tip[2]);
        } else if (state == STATE_TEST_CH_X) {
            showlog(QString("测试通道%1:按键").arg(test_chan));
        } else if (state == STATE_TEST_CH_X_RESULT) {
            showlog(QString("通道%1：按键已测试完毕").arg(test_chan));
        } else if (state == STATE_CALIB_ALL_RESULT) {
            showlog("测试结束");
        }
        break;

    default:
        break;
    }
}

void PressureSensorForm::startTask() {
    if (isTestContinue) {
        // ui->test_time->display(static_cast<double>(TestTime.elapsed()) / 1000.0);
        switch (state) {
        case STATE_IDLE: // 复位一切
            reset_all();
            calib_vector_init(product_model);
            qDebug() << "sizeof(sensor_v)" << sensor_v.size();

            showlog("当前mac地址为" + ui->macInput->text());
            state = STATE_WATI_CONNECT;
            break;

        case STATE_WATI_CONNECT: // 设置禁止休眠
            if (at->getConnected()) {
                showlog("蓝牙连接成功");
                protocolManager.get(DeviceCmd::BaseInfo); // 获取设备信息
                protocolManager.get(DeviceCmd::Sn, static_cast<int>(FacDevInfoType_TAIL_SN));
                state = STATE_DISABLE_SLEEP_CALIB;
            }
            break;

        case STATE_DISABLE_SLEEP_CALIB: // 禁止休眠
            delay_msec(100);
            if (pb->getState(Qpb::PbStateType::DisableSleep)) {
                showlog("已进入禁止休眠");
                protocolManager.set(DeviceCmd::PressCollect, static_cast<int>(FacSwitch_START));
                state = STATE_WATI_ASKQR;
            } else {
                delay_msec(500);
                protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
                showlog("再次发送禁止休眠");
            }
            break;

        case STATE_WATI_ASKQR:
            if (get_independent_state() == STATE_CALIB_START) {
                set_independent_state(STATE_CALIB_STATIC_WAIT);
                showlog("上位机开始请求治具控制");
                set_fixture_movement(product_model, STATE_WATI_ASKQR, 0);
                state = STATE_WAIT_STATIC;
            } else if (get_independent_state() == STATE_INVALID) {
                set_independent_state(STATE_WAIT_START);
                emit operator_instruct(getIndex());

                showlog("等待治具就绪" + SETTINGS.value("PRESSURE/functionSwitch", 1).toString());
                // if (SETTINGS.value("PRESSURE/functionSwitch", 1).toInt() == 2)
                //     state = STATE_WAIT_STATIC;
            }

            break;

        case STATE_WAIT_STATIC: // 收集无负载时的数据

            if (SETTINGS.value("SYSTEM/PressIndependent").toBool()) {
                set_independent_state(STATE_CALIB_STATIC_START);
            }
            if (pb->getState(Qpb::PbStateType::CollectParam) && isStartcali &&
                get_independent_state() == STATE_CALIB_STATIC_START) {
                delay_msec(200);

                protocolManager.get(DeviceCmd::GetPressCaliResult);

                showlog("已设置压感采集参数");

                if (function_switch == FUNCTION_CALIB_TEST || function_switch == FUNCTION_CALIB) {
                    showlog(donotmove);
                    for (int i = 0; i < sensor_v.size(); i++) {
                        sensor_v[i]->sensor_cali_set(1);
                    }
                    state = STATE_CALIB_CH_X;
                } else if (function_switch == FUNCTION_TEST) {
                    state = STATE_TEST_CH_X;
#if Y20_Pro_PRESS_TEST // Y20pro测试前需要校准系数阈值比对
                    protocolManager.get(DeviceCmd::GetPressCaliResult);
                    state = STATE_TEST_PARM_LIMIT;
#endif
                } else {
                    state = STATE_CALIB_CH_X;
                }
            }
            break;

        case STATE_CALIB_CH_X: //开始校准,治具动作

            if (sensor_v[calib_chan]->gs32SensorFlag == 2 && sensor_v[calib_chan]->calib_status != CALIB_NOCALIB) {
                set_fixture_movement(product_model, STATE_CALIB_CH_X, calib_chan);
                ui_msg_show(product_model, STATE_CALIB_CH_X, calib_chan);
                waittime->setInterval(measure_wait_time); // 超时判断计时开始
                waittime->start();
                showlog("校准超时计时开始,时间：" + QString::number(measure_wait_time));

                state = STATE_CALIB_CH_X_RESULT;
            } else if (sensor_v[calib_chan]->calib_status == CALIB_NOCALIB) {
                if (LastCali.calib_factor[sensor_v[calib_chan]->para.f_module[0]] != 0) {
                    switch (sensor_v[calib_chan]->para.f_module[0]) {
                    case MODULE_BTH:
                        sensor_v[calib_chan]->calib_result[0] = LastCali.calib_factor[MODULE_BTH];
                        sensor_v[calib_chan]->temperature = LastCali.temperature[MODULE_BTH];
                        break;

                    case MODULE_MODE_BUTTON:
                        sensor_v[calib_chan]->calib_result[0] = LastCali.calib_factor[MODULE_MODE_BUTTON];
                        break;

                    case MODULE_POWER_BUTTON:
                        sensor_v[calib_chan]->calib_result[0] = LastCali.calib_factor[MODULE_POWER_BUTTON];
                        break;
                    case MODULE_INVALID:
                    case MODULE_ASSISTANT_COMPONENT:
                    case MODULE_MAX:
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

        case STATE_CALIB_CH_X_RESULT: //开始校准，等待完成

            if (sensor_v[calib_chan]->calib_status == CALIB_SUC ||
                sensor_v[calib_chan]->calib_status == CALIB_NOCALIB) {
                ui_msg_show(product_model, STATE_CALIB_CH_X_RESULT, calib_chan);
                showlog(QString("通道%1已校准完毕").arg(calib_chan));
                set_fixture_movement(product_model, STATE_CALIB_CH_X_RESULT, calib_chan);
                if (calib_chan + 1 < total_sensor) {
                    calib_chan++;
                    state = STATE_CALIB_CH_X;
                    showlog(QString("通道%1开始校准").arg(calib_chan));
                } else {
                    state = STATE_CHECK_CAIL_RESULT;
                    showlog("所有通道校准完毕");
                }
                waittime->stop();
                qDebug() << "waittime->stop();";
            } else if (sensor_v[calib_chan]->gs32SensorFlag == 6) {
                protocolManager.set(DeviceCmd::PressCollect, static_cast<int>(FacSwitch_STOP));
                delay_msec(200);
                showlog("压感校准失败");
                waittime->stop();
                qDebug() << "stopime->stop();";
                result = failValue;
                state = STATE_SAVE_RESULT;
            }
            // 校准超时判断
            if (isovertime) {
                showlog(QString("校准超时"));
                state = STATE_OVERTIME_ERROR;
            }
            break;

        case STATE_CHECK_CAIL_RESULT: // 检查校准结果
            for (int chan = 0; chan < total_sensor; chan++) {
                if (sensor_v[chan]->para.lower_limit <= sensor_v[chan]->calib_result[0] &&
                    sensor_v[chan]->para.upper_limit >= sensor_v[chan]->calib_result[0]) {
                    showlog(QString("校准值在阈值范围内:%1-%2")
                                .arg(sensor_v[chan]->para.lower_limit)
                                .arg(sensor_v[chan]->para.upper_limit));
                    state = STATE_SEND_CAIL_RESULT;
                } else {
                    protocolManager.set(DeviceCmd::PressCollect, static_cast<int>(FacSwitch_STOP));
                    delay_msec(200);
                    showlog(QString("校准系数超阈值:%1(%2-%3)")
                                .arg(sensor_v[chan]->calib_result[0])
                                .arg(sensor_v[chan]->para.lower_limit)
                                .arg(sensor_v[chan]->para.upper_limit));
                    result = failValue;
                    state = STATE_SAVE_RESULT;
                    break;
                }
            }
            break;

        case STATE_SEND_CAIL_RESULT: // 开始发送校准值
            if (isCaliOk) {
                showlog("开始发送校准值");
                if (product_model == MODEL_ID_U7) {
                    if (ui->mydebug->checkState())
                        state = STATE_SAVE_CAIL_RESULT;
                    else
                        state = STATE_CHECK_NFC;
                } else {
                    state = STATE_SAVE_CAIL_RESULT;
                }
                calib_send_result(); // 发送校准值
            }

            break;

        case STATE_CHECK_NFC:
            if (CheckNfcData() == 6) {
                showlog("nfc标签校验成功");
                state = STATE_SAVE_CAIL_RESULT;
            } else {
                showlog("nfc标签校验失败");
                result = failValue;
                state = STATE_SAVE_RESULT;
            }

            if (ui->isusemes->checkState()) {
                if (stringsn == ui->getMac->text()) {
                    showlog("机内sn与mes比对校验成功");
                } else {
                    showlog("机内sn与mes比对校验失败");
                    result = failValue;
                    state = STATE_SAVE_RESULT;
                }
            }
            break;

        case STATE_SAVE_CAIL_RESULT: // 开始复位操作
            if (repeat_send_ok) {
                if (pb->get_is_save_press_cali_data()) // 接收校准存入完成
                {
                    delay_msec(500);
                    if (is_calib_suc == 1) { // 保存回读成功
                        showlog("已保存完毕");
                        delay_msec(500);
                        protocolManager.set(DeviceCmd::DevReset);
                        delay_msec(1000);

                        showlog("已发送复位请求，等待设备复位");
                        at->resetConnected();
                        if (function_switch == FUNCTION_CALIB_TEST || function_switch == FUNCTION_TEST) {
                            delay_msec(30);
                            at->set(DongleCmd::BleScanConnect, ui->macInput->text()); // 发送mac地址重连
                            showlog("已发送mac地址");
                            delay_msec(30);
                            state = STATE_WAIT_REST;
                        } else {
                            result = passValue;
                            state = STATE_SAVE_RESULT;
                        }
                    } else {
                        result = failValue;
                        showlog("写入失败，获取结果与写入结果不一致");
                        state = STATE_SAVE_RESULT;
                    }
                } else {
                    qDebug() << "主界面获取校准结果";
                    protocolManager.get(DeviceCmd::GetPressCaliResult);
                }
            }
            break;

            //////////////////////////////////////////////////////////////////////////////////////////////
            ///进入测试流程    ////////////////////////////////////////////////////

        case STATE_WAIT_REST: // 等待复位操作
            if (at->getConnected()) {
                showlog("蓝牙已重连");

                pb->reset_all_pb();

                state = STATE_DISABLE_SLEEP_TEST;
                // state = STATE_SAVE_RESULT;
            }
            break;

        case STATE_DISABLE_SLEEP_TEST: // 开始第二次禁止休眠
            // delay_msec(100);
            if (pb->getState(Qpb::PbStateType::DisableSleep)) {
                delay_msec(200);
                showlog("已进入第二次禁止休眠");
                protocolManager.set(DeviceCmd::PressCollect, static_cast<int>(FacSwitch_START));
                delay_msec(200);
                state = STATE_SET_COLLECT_PARAM_2;
            } else {
                showlog("设置第二次禁止休眠");
                protocolManager.set(DeviceCmd::ForbidSleep, static_cast<int>(FacSwitch_OPEN));
                delay_msec(200);
            }
            break;

        case STATE_SET_COLLECT_PARAM_2: // 开始第二次采集参数
            if (pb->getState(Qpb::PbStateType::CollectParam)) {
                delay_msec(200);
                showlog("已进入第二次设置压感采集参数");
                state = STATE_TEST_CH_X;
            }
            break;

        case STATE_TEST_PARM_LIMIT: // 开始第二次采集参数
            if (LastCali.calib_factor[MODULE_BTH] != 0) {
                for (int chan = 0; chan < total_sensor; chan++) {
                    if (sensor_v[chan]->para.lower_limit <= LastCali.calib_factor[chan + 1] &&
                        sensor_v[chan]->para.upper_limit >= LastCali.calib_factor[chan + 1]) {
                        showlog(QString("通道%3校准值在阈值范围内:%1-%2")
                                    .arg(sensor_v[chan]->para.lower_limit)
                                    .arg(sensor_v[chan]->para.upper_limit)
                                    .arg(chan));
                        state = STATE_TEST_CH_X;
                    } else {
                        protocolManager.set(DeviceCmd::PressCollect, static_cast<int>(FacSwitch_STOP));
                        delay_msec(200);
                        showlog(QString("通道%3校准系数超阈值:%1(%2-%3)")
                                    .arg(LastCali.calib_factor[chan])
                                    .arg(sensor_v[chan]->para.lower_limit)
                                    .arg(sensor_v[chan]->para.upper_limit)
                                    .arg(chan));
                        result = failValue;
                        state = STATE_SAVE_RESULT;
                        break;
                    }
                }
            }
            break;

        case STATE_TEST_CH_X: //测试通道x

            if (sensor_v[test_chan]->test_status == TEST_INVALID) {
                showlog("设置为TEST_START");
                sensor_v[test_chan]->sensor_test_status_set(TEST_START);
                set_fixture_movement(product_model, STATE_TEST_CH_X, test_chan);
                ui_msg_show(product_model, STATE_TEST_CH_X, test_chan);

                showlog("开始计时测试时间" + QString::number(measure_wait_time));

                waittime->setInterval(measure_wait_time);
                waittime->start();

                state = STATE_TEST_CH_X_RESULT;
            } else if (sensor_v[test_chan]->test_status == TEST_NOTEST) {
                state = STATE_TEST_CH_X_RESULT;
            }

            break;

        case STATE_TEST_CH_X_RESULT: //测试通道x结果

            if (sensor_v[test_chan]->test_status == TEST_SUC || sensor_v[test_chan]->test_status == TEST_NOTEST) {
                waittime->stop();
                ui_msg_show(product_model, STATE_TEST_CH_X_RESULT, test_chan);
                set_fixture_movement(product_model, STATE_TEST_CH_X_RESULT, test_chan);
                if (test_chan + 1 < total_sensor) {
                    test_chan++;
                    state = STATE_TEST_CH_X;
                    showlog(QString("通道%1开始测试").arg(test_chan));
                } else {
                    state = STATE_TEST_ALL_RESULT;
                    showlog("所有通道测试完毕");
                }
            } else if (sensor_v[test_chan]->test_status == TEST_FAIL) {
                waittime->stop();
                protocolManager.set(DeviceCmd::PressCollect, static_cast<int>(FacSwitch_STOP));
                delay_msec(200);
                showlog("压感测试失败");
                result = failValue;
                state = STATE_SAVE_RESULT;
            }

            if (isovertime) {
                showlog(QString("测试超时"));
                state = STATE_OVERTIME_ERROR;
            }
            break;

        case STATE_TEST_ALL_RESULT: //测试ALL通道结果
            if (1) {
                ui_msg_show(product_model, STATE_TEST_ALL_RESULT, test_chan);

                result = passValue;
                if (SETTINGS.value("Press/AmplitudeLimit", false).toBool()) {
                    state = STATE_AMPLITUEDE;
                    delay_msec(1000);
                    send_start_command(COMMAND_ID_FAMA_200_O, 0);
                } else
                    state = STATE_SAVE_RESULT;
            }
            break;
        case STATE_AMPLITUEDE:
            if (Amplituderesult == 0) {
                waitWork(5000);
                jig->get_amplitude();
            } else if (Amplituderesult == 1) {
                state = STATE_SAVE_RESULT;
            } else {
                result = failValue;
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
                    emit send_end_test_pass(pack);
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
                    emit send_end_test_pass(pack);
                }
            }
            set_fixture_movement(product_model, STATE_SAVE_RESULT, 0);
            save_press_test_data_to_csv(macAddress, cali_result);
            isTestContinue = false;
            // showlog("压感测试完毕");

            protocolManager.set(DeviceCmd::DevReset); // 开始复位设备
            delay_msec(50);
            showlog("设备已复位");
            closeDongleSerialPort();

            stringsn = "";

            ui->macInput->setDisabled(0);
            ui->getMac->setDisabled(0);
            ui->macInput->clear();
            emit send_end_test(getIndex());
            ui->getMac->clear();

            showlog("流程结束");
            if (pack.factory == "lx" && m_index == 1)
                ui->getMac->setFocus();

            if (pack.factory == "hq" && m_index == 1)
                ui->getMac->setFocus();
            state = STATE_IDLE;

            break;

        default:
            break;
        }

        //  QCoreApplication::processEvents();
    }
}

void PressureSensorForm::on_ClearGraph_clicked() {
    graph_reset(0);
}

void PressureSensorForm::on_GetButton_clicked() {
    protocolManager.get(DeviceCmd::ButtonState, 1);
    return;
}

void PressureSensorForm::overTask() {
    on_end_clicked();
}

void PressureSensorForm::on_button_get_calib_factor_clicked() {
    delay_msec(200);
    protocolManager.get(DeviceCmd::GetPressCaliResult);
    delay_msec(200);
}

uint8_t PressureSensorForm::CheckNfcData() {
    uint8_t ret = 0;
    QString ReadNfcData = ReadNfcDataProcess();
    if (ReadNfcData == 0)
        return 0;
    // 定义各部分的长度
    int lenA = 32; // A 的长度
    int lenB = 2;  // B 的长度
    int lenC = 24; // C 的长度
    int lenD = 32; // D 的长度
    int lenE = 20; // E 的长度
    int lenF = 12; // F 的长度
    int lenG = 2;  // G 的长度

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

    for (int i = 4; i * 4 < dataSize; i += 4) { // 每次处理16个字节
        st = dc_read(icdev, i, rdata);
        if (st != 0) {
            // showlog("dc_read Error!");
            showlog("nfc信息读取失败");
            return 0;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            //   showlog(QString::fromStdString(str1));
        }
    }
    if (dataSize % 16) {
        st = dc_read(icdev, 4 + (dataSize / 16) * 4, rdata);
        if (st != 0) {
            showlog("nfc信息读取失败");
            //  showlog("dc_read Error!");
            return 0;
        } else {
            memset(rdatahex, 0x00, sizeof(rdatahex));
            hex_a(rdata, rdatahex, dataSize % 16);
            std::string str1 = (char*)rdatahex;
            ReadNfcData = ReadNfcData + QString::fromStdString(str1);
            //  showlog(QString::fromStdString(str1));
        }
    }

    showlog("nfc内容为：" + ReadNfcData);

    return ReadNfcData;
}

void PressureSensorForm::refreshBaseData(ProtocolBaseInfoData data) {
    readProduct = data.product_name;
    qDebug() << "refreshBaseData";
    showlog("获取到当前型号为：" + readProduct);
}

void PressureSensorForm::on_TestButtonNFC_clicked() {
    QString ReadNfcData = ReadNfcDataProcess();
}

void PressureSensorForm::on_TestButton1_clicked() {
    // protocolManager.get(DeviceCmd::BaseInfo);  // 获取设备信息

    // CheckNfcData();

    // qDebug() << "压感结果" << sensor_v[0]->para.lower_limit << sensor_v[1]->para.lower_limit;

    jig->get_amplitude();
}
void PressureSensorForm::refreshAmplitudeData(QString data) {
    showlog("获取到摆幅为：" + data);

    // 检查是否启用了摆幅限制
    if (SETTINGS.value("Press/AmplitudeLimit", false).toBool()) {
        int amplitudeUpper = SETTINGS.value("Press/AmplitudeLimitUpper", 0).toInt();
        int amplitudeLower = SETTINGS.value("Press/AmplitudeLimitLower", 0).toInt();
        int currentAmplitude = data.toInt();

        if (currentAmplitude < amplitudeLower || currentAmplitude > amplitudeUpper) {
            showlog("摆幅超出限制范围：" + QString::number(amplitudeLower) + " 至 " + QString::number(amplitudeUpper));
            Amplitudetimes++;

            if (Amplitudetimes > 3) {
                Amplitudetimes = 0;
                Amplituderesult = 2;
            }

        } else {
            showlog("摆幅满足要求" + QString::number(amplitudeLower) + " 至 " + QString::number(amplitudeUpper));
            Amplituderesult = 1;
        }
    }
}
