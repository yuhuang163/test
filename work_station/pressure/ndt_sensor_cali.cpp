#include "ndt_sensor_cali.h"

#include <QSettings>

#include "AbIni.h"
#include "qdebug.h"
#if _MSC_VER >= 1600
#    pragma execution_character_set(push, "utf-8")
#endif

ndt_sensor_cali::ndt_sensor_cali() {
    brush_test_time = SETTINGS.value("pressure/brush_test_time", "600").toInt();
    botton_test_time = SETTINGS.value("pressure/botton_test_time", "600").toInt();

    gs32SensorFlag = cal_no_flag;
    noiseLoopTime = 0;
    gu8CaliReset = 0;
    cycles = 0;
    calProcessFlag = cal_no_flag;

    memset(gSensorPressAdc, 0, sizeof(gSensorPressAdc));
    memset(rawDataFactor, 0, sizeof(rawDataFactor));
    memset(iDiffSquSum, 0, sizeof(iDiffSquSum));
    memset(gNoiseDetectCount, 0, sizeof(gNoiseDetectCount));
    memset(diffMaxFlag, 0, sizeof(diffMaxFlag));

    memset(err, 0, sizeof(err));
    memset(noiseMax, 0, sizeof(noiseMax));
    memset(adcHead, 0, sizeof(adcHead));
    memset(singleFlag, 0, sizeof(singleFlag));
    memset(first_adc, 0, sizeof(first_adc));

    memset(adcHeadSum, 0, sizeof(adcHeadSum));
    memset(diffData, 0, sizeof(diffData));
    memset(diffDataSum, 0, sizeof(diffDataSum));
    memset(diffDataCal, 0, sizeof(diffDataCal));
    memset(noisePeak, 0, sizeof(noisePeak));

    memset(noiseStd, 0, sizeof(noiseStd));
    memset(noiseAdcBuf, 0, sizeof(noiseAdcBuf));
    memset(noiseAdcBufSum, 0, sizeof(noiseAdcBufSum));
    memset(noiseAdcDiffSum, 0, sizeof(noiseAdcDiffSum));
}

/*
* Description   : 噪声计算
* Paramter      : adc:导入ADC数值数值
                  noisePeak：数据最大差值
                  noiseStd： 数据方差
* Return        : 0:未完成 1：完成
*/

unsigned char ndt_sensor_cali::Noise_CalProc(short* adc, short* noisePeak, short* noiseStd) {
    unsigned char ret = 0;
    short diffData[CHANNEL_MAX];
    short ave[CHANNEL_MAX] = {0};
    short dataPeak[CHANNEL_MAX] = {0};

    if (noiseLoopTime++ < NOISE_BUF_COUNT) {
        for (unsigned char ch = 0; ch < CAL_CHANNEL_NUM; ch++) {
            noiseAdcBuf[ch][noiseLoopTime - 1] = adc[ch];
            noiseAdcBufSum[ch] += adc[ch];
        }

        ret = 0;
    } else {
        for (unsigned char ch = 0; ch < CAL_CHANNEL_NUM; ch++) {
            ave[ch] = noiseAdcBufSum[ch] / NOISE_BUF_COUNT;

            for (short i = 0; i < NOISE_BUF_COUNT; i++) {
                diffData[ch] = noiseAdcBuf[ch][i] - ave[ch];
                noiseAdcDiffSum[ch] += diffData[ch] * diffData[ch];
                if (dataPeak[ch] < qAbs(diffData[ch])) {
                    dataPeak[ch] = qAbs(diffData[ch]);
                }
            }

            noisePeak[ch] = dataPeak[ch];
            noiseStd[ch] = noiseAdcDiffSum[ch];
        }
        ret = 1;
    }

    return ret;
}

void ndt_sensor_cali::sensor_cali_set(uint8_t stat)  // 开关
{
    gu8CaliReset = stat;
}

void ndt_sensor_cali::sensor_test_status_set(TEST_STATUS_E t_status) { test_status = t_status; }

void ndt_sensor_cali::Sensor_cali_Init(CAL_CHANNEL_E machine)  // 根据不同机型进行初始化
{
    switch (machine) {
        case CAL_CHANNEL_F20_CH0:  // f20_bth
            CAL_CHANNEL_NUM = 1;   // 校准通道数
            CAL_SIGNAL_CH0 = 70;   // 差异值刷头
            CAL_WEIGHT_CH0 = 200;  // 刷头校准质量(g)

            para.lower_limit = 1000;
            para.upper_limit = 9000;

            ui_msg_tip[0] = donotmove;
            ui_msg_tip[1] = "人员：刷头放砝码";
            ui_msg_tip[2] = "人员：拿走砝码";

            // 测试的msg提醒
            ui_msg_test[0] = "人员：请放50g砝码";
            ui_msg_test[1] = "人员：请放350g砝码";
            ui_msg_test[2] = "人员：请放450g砝码";
            break;

        case CAL_CHANNEL_F20_CH1:  // f20_key1
            CAL_CHANNEL_NUM = 1;   // 校准通道数
            CAL_SIGNAL_CH0 = 10;   // 差异值按键
            CAL_WEIGHT_CH0 = 200;  // 按键校准质量(g)

            para.lower_limit = 1000;
            para.upper_limit = 15000;

            ui_msg_tip[0] = donotmove;
            ui_msg_tip[1] = "人员：模式按键放砝码";
            ui_msg_tip[2] = "人员：拿走砝码";

            // 测试的msg提醒
            ui_msg_test[0] = "开始测试模式按键";
            break;

        case CAL_CHANNEL_F20_CH2:  // f20_key2
            CAL_CHANNEL_NUM = 1;   // 校准通道数
            CAL_SIGNAL_CH0 = 10;   // 差异值按键
            CAL_WEIGHT_CH0 = 200;  // 按键校准质量(g)

            para.lower_limit = 1000;
            para.upper_limit = 15000;

            ui_msg_tip[0] = donotmove;
            ui_msg_tip[1] = "人员：电源按键放砝码";
            ui_msg_tip[2] = "人员：拿走砝码";

            // 测试的msg提醒
            ui_msg_test[0] = "开始测试电源按键";
            break;

        case CAL_CHANNEL_Y20_CH0:  // y20p_bth
            CAL_CHANNEL_NUM = 1;   // 校准通道数

            CAL_SIGNAL_CH0 = 300;  // 差异值刷头

            CAL_WEIGHT_CH0 = 400;  // 刷头校准质量(g)
            break;

        case CAL_CHANNEL_Y20_CH1:  // y20p_key
            CAL_CHANNEL_NUM = 1;   // 校准通道数

            CAL_SIGNAL_CH0 = 40;  // 差异值按键

            CAL_WEIGHT_CH0 = 200;  // 按键校准质量(g)
            break;

        case CAL_CHANNEL_U7_CH0:
            CAL_CHANNEL_NUM = 1;  // 校准通道数

            CAL_SIGNAL_CH0 = 40;  // 差异值按键

            CAL_WEIGHT_CH0 = 300;  // 按键校准质量(g)
            break;

        case CAL_CHANNEL_Y21_CH0:
            CAL_CHANNEL_NUM = 1;  // 校准通道数

            CAL_SIGNAL_CH0 = 250;  // 差异值刷头

            CAL_WEIGHT_CH0 = 400;  // 按键刷头质量(g)
            break;

        case CAL_CHANNEL_P30P_CH0:
            CAL_CHANNEL_NUM = 1;   // 校准通道数
            CAL_SIGNAL_CH0 = 40;   // 差异值按键
            CAL_WEIGHT_CH0 = 300;  // 按键刷头质量(g)
            break;
        case CAL_CHANNEL_Y30PS_CH0:
            CAL_CHANNEL_NUM = 1;  // 校准通道数
            product = CAL_CHANNEL_Y30PS_CH0;
            CAL_SIGNAL_CH0 = 80;                  // 差异值按键
            CAL_SIGNAL_CH1 = CAL_SIGNAL_CH0 / 5;  // 差异值辅助元器件
            CAL_WEIGHT_CH0 = 400;                 // 按键刷头质量(g)
            CAL_WEIGHT_CH1 = 400;                 // 按键刷头质量(g)
            break;
        case CAL_CHANNEL_Y20PO_CH0:
            CAL_CHANNEL_NUM = 1;  // 校准通道数
            product = CAL_CHANNEL_Y20PO_CH0;
            CAL_SIGNAL_CH0 = 20;                  // 差异值按键
            CAL_SIGNAL_CH1 = CAL_SIGNAL_CH0 / 5;  // 差异值辅助元器件
            CAL_WEIGHT_CH0 = 200;                 // 按键刷头质量(g)
            CAL_WEIGHT_CH1 = 200;                 // 按键刷头质量(g)

            break;

        default: break;
    }
}
/*
* Description   : 校准流程
* Paramter      : adc: 输入获取到的adc
                  gRawDataFactor:返回的校准系数
                  err：错误代码
* Return        : 1不要移动 2 通道一按下 3 通道1抬起 4 通道二按下 5通道二抬起  6校准完成
*/
CAL_FLAG_STATE ndt_sensor_cali::Calibration_Proces_v2(short* adc, unsigned short* gRawDataFactor, short* err) {
    u32 cal_temp = 0;
    // qDebug() << "adc[CH0]:" << adc[CH0];
    // qDebug() << "adc[CH1]:" << adc[CH1];
    if (cycles++ <= CAL_SELF_CHECKING_TIME)  // 200帧用于噪声判断
    {
        Self_Checking_Proces(adc, err);
        calProcessFlag = cal_self_checking_flag;
        if (cycles > CAL_SELF_CHECKING_TIME - SMOOTH_COUNT) {
            adcHeadSum[CH0] += adc[CH0];
            adcHeadSum[CH1] += adc[CH1];
        }
        if (cycles == CAL_SELF_CHECKING_TIME) {
            adcHead[CH0] = adcHeadSum[CH0] / SMOOTH_COUNT;
            adcHead[CH1] = adcHeadSum[CH1] / SMOOTH_COUNT;
        }
    } else if (cycles <= (CAL_SELF_CHECKING_TIME + CAL_CH0_USE_TIME) && cycles > CAL_SELF_CHECKING_TIME) {
        diffData[CH0] = adc[CH0] - adcHead[CH0];
        diffData[CH1] = adc[CH1] - adcHead[CH1];
        qDebug() << "diffData[CH0]:" << diffData[CH0];
        qDebug() << "diffData[CH1]:" << diffData[CH1];
        if (diffData[CH0] > CAL_SIGNAL_CH0) {
            diffMaxFlag[CH0]++;
        } else {
            diffDataSum[CH0] = 0;
            diffMaxFlag[CH0] = 0;
        }

        if (diffData[CH1] < CAL_SIGNAL_CH1 && diffData[CH1] > 0) {
            diffMaxFlag[CH1]++;
        } else {
            diffDataSum[CH1] = 0;
            diffMaxFlag[CH1] = 0;
        }
        // qDebug() << " Calibration_Proces_v2的singleFlag[CH0]" << singleFlag[CH0];
        if (singleFlag[CH0] == 0) {
            calProcessFlag = cal_ch0_press_flag;
        }

        if (diffMaxFlag[CH0] > CAL_SIGNAL_TIME - SMOOTH_COUNT) {  //大于180
            diffDataSum[CH0] += diffData[CH0];
            // qDebug() << " diffDataSum[CH1]" << diffDataSum[CH1];
            // qDebug() << "diffData[CH1]:" << diffData[CH1];
            diffDataSum[CH1] += diffData[CH1];
            qDebug() << " diffDataSum[CH1]" << diffDataSum[CH1];
            // printf("diffData:%d\r\n",diffData[CH0]);
            if (diffMaxFlag[CH0] == CAL_SIGNAL_TIME) {  //等于200

                if (diffDataSum[CH0] < 20)
                    diffDataSum[CH0] = 20;
                if (diffDataSum[CH1] < 320)
                    diffDataSum[CH1] = 320;

                diffDataCal[CH0] = diffDataSum[CH0] / SMOOTH_COUNT;
                diffDataCal[CH1] = diffDataSum[CH1] / SMOOTH_COUNT;
                calProcessFlag = cal_ch0_leave_flag;
                singleFlag[CH0] = 1;
                // qDebug() << "CH0 - CAL_WEIGHT_CH0:" << CAL_WEIGHT_CH0 << " diffDataCal[CH0]:" << diffDataCal[CH0]
                //          << " cal_temp:" << (CAL_WEIGHT_CH0 * 1024 / diffDataCal[CH0])
                //          << " gRawDataFactor[CH0]:" << gRawDataFactor[CH0];

                // qDebug() << "CH1 - CAL_WEIGHT_CH1:" << CAL_WEIGHT_CH1;
                qDebug() << "diffDataCal[CH1]:" << diffDataCal[CH1];
                qDebug() << " diffDataSum[CH1]" << diffDataSum[CH1];
                qDebug() << "CAL_WEIGHT_CH1：" << CAL_WEIGHT_CH1;
                // qDebug() << "cal_temp:" << cal_temp;
                // qDebug() << "gRawDataFactor[CH1]:" << gRawDataFactor[CH1];

                // qDebug() << "CH1 - CAL_WEIGHT_CH1:" << CAL_WEIGHT_CH1 << " diffDataCal[CH1]:" << diffDataCal[CH1]
                //          << " cal_temp:" << (CAL_WEIGHT_CH1 * 1024 / diffDataCal[CH1])
                //          << " gRawDataFactor[CH1]:" << gRawDataFactor[CH1];

                cal_temp = CAL_WEIGHT_CH0 * 1024 / diffDataCal[CH0];
                gRawDataFactor[CH0] = cal_temp;

                cal_temp = CAL_WEIGHT_CH1 * 1024 / diffDataCal[CH1];
                qDebug() << "CAL_WEIGHT_CH1:" << CAL_WEIGHT_CH1;
                qDebug() << "diffDataCal[CH1]:" << diffDataCal[CH1];
                qDebug() << "cal_temp:" << cal_temp;

                gRawDataFactor[CH1] = cal_temp;
                qDebug() << "gRawDataFactor[CH0]:" << gRawDataFactor[CH0];
                qDebug() << "gRawDataFactor[CH1]:" << gRawDataFactor[CH1];
                if (diffMaxFlag[CH1] > SMOOTH_COUNT) {
                    qDebug() << " 辅助元件校准ok";
                    singleFlag[CH1] = 1;
                }
                // printf("diffDataSum:%d %d diffDataCal:%d %d\r\n",diffDataSum[CH0],diffDataSum[CH1],diffDataCal[CH0],
                // diffDataCal[CH1]);
            }
        }
    } else if (cycles > (CAL_SELF_CHECKING_TIME + CAL_CH0_USE_TIME)) {
        calProcessFlag = cal_finally_flag;
        if (singleFlag[CH0] == 0) {
            err[CH0] = err[CH0] | 0x08;
        }
        if (singleFlag[CH1] == 0) {
            err[CH1] = err[CH1] | 0x08;
        }
    }

    return calProcessFlag;
}

/*
* Description   : 校准流程
* Paramter      : adc: 输入获取到的adc
                  gRawDataFactor:返回的校准系数
                  err：错误代码
* Return        : 1不要移动 2 挂砝码 3 拿起砝码 4 按键按下 5挺起按键（校准完成）
*/
CAL_FLAG_STATE ndt_sensor_cali::Calibration_Proces_ndt(short* adc, unsigned short* gRawDataFactor, short* err) {
    short diffData[CHANNEL_MAX] = {0};
    unsigned int cal_temp = 0;
    // qDebug() << "cycles=" << cycles;
    if (cycles++ <= CAL_SELF_CHECKING_TIME)  // 200帧用于噪声判断
    {
        Self_Checking_Proces(adc, err);
        calProcessFlag = cal_self_checking_flag;
        if (cycles > CAL_SELF_CHECKING_TIME - SMOOTH_COUNT) {
            adcHeadSum[CH0] += adc[CH0];
            if (CAL_CHANNEL_NUM == 2) {
                adcHeadSum[CH1] += adc[CH1];
            }
        }
        if (cycles == CAL_SELF_CHECKING_TIME) {
            adcHead[CH0] = adcHeadSum[CH0] / SMOOTH_COUNT;
            if (CAL_CHANNEL_NUM == 2) {
                adcHead[CH1] = adcHeadSum[CH1] / SMOOTH_COUNT;
            }
            // printf("adcHead %d %d\r\n",adcHead[CH0],adcHead[CH1]);
        }
    } else if (cycles <= (CAL_SELF_CHECKING_TIME + CAL_CH0_USE_TIME) &&
               cycles > CAL_SELF_CHECKING_TIME) {  // 200到800之间
        diffData[CH0] = adc[CH0] - adcHead[CH0];
        if (diffData[CH0] > CAL_SIGNAL_CH0)  // 如果有超过200adc
        {
            diffMaxFlag[CH0]++;
        } else {
            diffDataSum[CH0] = 0;
            diffMaxFlag[CH0] = 0;
        }
        qDebug() << " singleFlag[CH0]" << singleFlag[CH0];
        if (singleFlag[CH0] == 0) {
            calProcessFlag = cal_ch0_press_flag;
        }
        if (diffMaxFlag[CH0] > CAL_SIGNAL_TIME - SMOOTH_COUNT) {
            diffDataSum[CH0] += diffData[CH0];
            // printf("diffData:%d\r\n",diffData[CH0]);
            if (diffMaxFlag[CH0] == CAL_SIGNAL_TIME) {
                diffDataCal[CH0] = diffDataSum[CH0] / SMOOTH_COUNT;
                calProcessFlag = cal_ch0_leave_flag;
                singleFlag[CH0] = 1;
                cal_temp = CAL_WEIGHT_CH0 * 1024 / diffDataCal[CH0];
                gRawDataFactor[CH0] = cal_temp;
                // printf("diffDataSum:%d diffDataCal:%d\r\n",diffDataSum[CH0], diffDataCal[CH0]);
            }
        }
    }

    if (CAL_CHANNEL_NUM == 2) {
        if (cycles < (CAL_SELF_CHECKING_TIME + CAL_CH0_USE_TIME + CAL_CH1_USE_TIME) &&
            cycles > (CAL_SELF_CHECKING_TIME + CAL_CH0_USE_TIME)) {
            diffData[CH1] = adc[CH1] - adcHead[CH1];
            if (diffData[CH1] > CAL_SIGNAL_CH1) {
                diffMaxFlag[CH1]++;
            } else {
                diffMaxFlag[CH1] = 0;
                diffDataSum[CH1] = 0;
            }

            if (singleFlag[CH1] == 0) {
                calProcessFlag = cal_ch1_press_flag;
            }

            if (diffMaxFlag[CH1] > CAL_SIGNAL_TIME - SMOOTH_COUNT) {
                diffDataSum[CH1] += diffData[CH1];
                // printf("diffData:%d\r\n",diffData[CH1]);
                if (diffMaxFlag[CH1] == CAL_SIGNAL_TIME) {
                    diffDataCal[CH1] = diffDataSum[CH1] / SMOOTH_COUNT;
                    calProcessFlag = cal_ch1_leave_flag;
                    singleFlag[CH1] = 1;
                    cal_temp = CAL_WEIGHT_CH1 * 1024 / diffDataCal[CH1];
                    gRawDataFactor[CH1] = cal_temp;
                    // printf("diffDataSum:%d diffDataCal:%d\r\n",diffDataSum[CH1], diffDataCal[CH1]);
                }
            }
        }
        if (cycles > (CAL_SELF_CHECKING_TIME + CAL_CH0_USE_TIME + CAL_CH1_USE_TIME)) {
            calProcessFlag = cal_finally_flag;
            if (singleFlag[CH0] == 0) {
                err[CH0] = err[CH0] | 0x08;
            }
            if (singleFlag[CH1] == 0) {
                err[CH1] = err[CH1] | 0x08;
            }
        }
    } else if (CAL_CHANNEL_NUM == 1) {
        if (cycles > (CAL_SELF_CHECKING_TIME + CAL_CH0_USE_TIME)) {
            calProcessFlag = cal_finally_flag;
            if (singleFlag[CH0] == 0) {
                err[CH0] = err[CH0] | 0x08;
            }
        }
    } else {
        qDebug() << "CAL_CHANNEL_NUM_error";
    }
    qDebug() << "calProcessFlag=" << calProcessFlag;
    return calProcessFlag;
}
/*
* Description   : 校准流程
* Paramter      : adc: 输入获取到的adc
                  gRawDataFactor:返回的校准系数
                  err：错误代码
* Return        : 1不要移动 2 通道一按下 3 通道1抬起 4 通道二按下 5通道二抬起  6校准完成
*/
unsigned short* ndt_sensor_cali::ndt_sensor_cali_process(int count, short* adc) {
    if (gu8CaliReset) {
        if (count > 0)  // 有数据
        {
            gSensorPressAdc[0] = adc[0];
            if (CAL_CHANNEL_NUM == 2) {
                gSensorPressAdc[1] = adc[1];
            }
            qDebug() << "按键的adc " << adc[0];       // 刷头
            qDebug() << "辅助元器件的adc" << adc[1];  // 按键

            // qDebug() << "CSU18M68, Cali BrushHead: PressAdc = " << gSensorPressAdc[0];  // 刷头
            // qDebug() << "CSU18M68, Cali Key: PressAdc =" << gSensorPressAdc[1];         // 按键

            // printf("CSU18M68, Cali Temperature: %d\r\n", tempFactor);
            // qDebug() << "product为" << product;
            if (product == CAL_CHANNEL_Y30PS_CH0 || product == CAL_CHANNEL_Y20PO_CH0)
                gs32SensorFlag = Calibration_Proces_v2(adc, rawDataFactor, err);
            else
                gs32SensorFlag = Calibration_Proces_ndt(adc, rawDataFactor, err);

            if (product == CAL_CHANNEL_Y30PS_CH0 || product == CAL_CHANNEL_Y20PO_CH0) {
                qDebug() << "双通道，ndt算法返回值" << gs32SensorFlag;
                if (gs32SensorFlag == cal_self_checking_flag) {
                    qDebug() << "别移动,正在获取0点ADC";
                } else if (gs32SensorFlag == cal_ch0_press_flag) {
                    qDebug() << "按键放200g砝码";
                } else if (gs32SensorFlag == cal_ch0_leave_flag) {
                    if (CAL_CHANNEL_NUM == 1) {
                        qDebug() << "校准事件完成";

                        if (err[CH0] == 0 || err[CH1] == 0) {
                            qDebug() << "校准通过";
                            return rawDataFactor;
                        } else {
                            qDebug() << "校准失败";
                            gSensorPressAdc[0] = err[CH0];
                            gSensorPressAdc[1] = err[CH1];
                            rawDataFactor[0] = 0;
                            qDebug() << "发生错误：CH0_err =" << err[CH0];
                            qDebug() << "发生错误：CH1_err =" << err[CH1];
                            gs32SensorFlag = cal_finally_flag;  // 失败
                        }
                        gu8CaliReset = 0;
                    }
                }
            } else {
                qDebug() << "单通道，ndt算法返回值" << gs32SensorFlag;
                if (gs32SensorFlag == cal_self_checking_flag) {
                    qDebug() << "别移动,正在获取0点ADC";
                } else if (gs32SensorFlag == cal_ch0_press_flag) {
                    qDebug() << "校准通道0";
                } else if (gs32SensorFlag == cal_ch0_leave_flag) {
                    if (CAL_CHANNEL_NUM == 1) {
                        qDebug() << "校准事件完成";

                        if (err[0] == 0) {
                            qDebug() << "校准通过";
                            return rawDataFactor;
                        } else {
                            qDebug() << "校准失败";
                            gSensorPressAdc[0] = err[0];
                            rawDataFactor[0] = 0;
                            qDebug() << "发生错误：err =" << err[0];
                            gs32SensorFlag = cal_finally_flag;  // 失败
                        }
                        gu8CaliReset = 0;
                    } else {
                        qDebug() << "拿走砝码";
                    }
                } else if (gs32SensorFlag == cal_ch1_press_flag) {
                    qDebug() << "按键放xg砝码";
                    // Cailbration_Coef_Write();
                } else if (gs32SensorFlag == cal_ch1_leave_flag) {
                    qDebug() << "校准事件完成";
                    //  QThread::msleep(2000); // 暂停当前线程 2000 毫秒（2秒）

                    if (err[0] == 0 && err[1] == 0) {
                        qDebug() << "校准通过";
                        return rawDataFactor;
                    } else {
                        qDebug() << "校准失败";
                        gSensorPressAdc[0] = err[0];
                        gSensorPressAdc[1] = err[1];
                        rawDataFactor[0] = 0;
                        rawDataFactor[1] = 0;
                        qDebug() << "发生错误: err =" << err[0] << err[1];
                        gs32SensorFlag = cal_finally_flag;  // 失败
                    }

                    gu8CaliReset = 0;
                }
            }
        }
    }

    return rawDataFactor;
}
/*
* Description   : 自检流程
* Paramter      : adc: 输入获取到的adc
                  err：错误代码
* Return        : 0未完成 1完成
*/
unsigned char ndt_sensor_cali::Self_Checking_Proces(short* adc, short* err) {
    unsigned char ret = 0;
    unsigned char ch = 0;

    for (ch = 0; ch < CAL_CHANNEL_NUM; ch++) {
        if (adc[ch] < OFFSET_ADC_NEGATIVE || adc[ch] > OFFSET_ADC_POSITIVE) {
            err[ch] = err[ch] | 0x01;
        }
    }
    if (Noise_CalProc(adc, noisePeak, noiseStd)) {
        // printf("noiseStd:%d %d noisePeak:%d %d\r\n",noiseStd[0],noiseStd[1], noisePeak[0],
        // noisePeak[1]);

        for (ch = 0; ch < CAL_CHANNEL_NUM; ch++) {
            if (noiseStd[ch] > NOISE_STD_STANDARD) {
                err[ch] = err[ch] | 0x02;
            }
            if (noisePeak[ch] > NOISE_PEAK_STANDARD) {
                err[ch] = err[ch] | 0x04;
            }
        }
        ret = 1;
    }

    return ret;
}
