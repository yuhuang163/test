#ifndef NDT_SENSOR_CALI_H
#define NDT_SENSOR_CALI_H
#include <QMainWindow>
#include <QThread>
#include "qpb.h"

#define CHANNEL_MAX 2

#define CAL_SIGNAL_TIME        200

#define CAL_SELF_CHECKING_TIME NOISE_BUF_COUNT
#define CAL_CH0_USE_TIME       600
#define CAL_CH1_USE_TIME       600

#define NOISE_BUF_COUNT 200
#define SMOOTH_COUNT    20

#define CH0 0
#define CH1 1

#define OFFSET_ADC_POSITIVE 27000    // adc, 3V+250mV
#define OFFSET_ADC_NEGATIVE -27000   // adc, 3V+250mV
#define NOISE_PEAK_STANDARD 8        // PTP
#define NOISE_STD_STANDARD  800      // sdt,NOISE_BUF_COUNT

#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif

typedef enum
{
    CAL_CHANNEL_INVILD = 0,

    CAL_CHANNEL_Y20_CH0,
    CAL_CHANNEL_Y20_CH1,
    CAL_CHANNEL_Y21_CH0,
    CAL_CHANNEL_F20_CH0,
    CAL_CHANNEL_F20_CH1,
    CAL_CHANNEL_F20_CH2,
    CAL_CHANNEL_U7_CH0,
    CAL_CHANNEL_P30P_CH0,

    CAL_CHANNEL_MAX
} CAL_CHANNEL_E;

typedef enum
{
    CALIB_INVALID = 0,
    CALIB_NOCALIB,
    CALIB_START,
    CALIB_FAILED,
    CALIB_SUC,
    CALIB_STATUS_MAX
} CALIB_STATUS_E;

typedef enum
{
    TEST_INVALID = 0,
    TEST_NOTEST,
    TEST_START,
    TEST_BTH_SHAKE,
    TEST_BTH_NORMAL,
    TEST_BTH_OVERPRESS,
    TEST_BTH_END,
    TEST_KET_NORMAL,
    TEST_KET_NO_CLICK,
    TEST_KET_CLICK,
    TEST_FAIL,
    TEST_SUC,
    TEST_STATUS_MAX

} TEST_STATUS_E;

class ndt_sensor_cali
{
public:
    unsigned int calib_result = 0;
    unsigned int temperature = 0;

    typedef struct {
        press_module_e f_module;
        unsigned int upper_limit;   // 校准系数上限
        unsigned int lower_limit;   // 校准系数下限
        int16_t adc_threshold;      // adc变化量阈值，达到阈值之后才开始校准
        int16_t count_threshold;    // 超过adc阈值之后开始计数，超过该阈值之后才开始校准
        int32_t first_adc;          // 第一次的adc值

        int16_t test_grams[3] = {0};    // 测试压力
        int16_t test_tolerance[3] = {0};// 测试容差
        int16_t test_count[3] = {0};    // 测试计数
        int16_t current_count = 0; //correct
        int16_t button_state = 0; //correct
        int32_t no_click_max = 0;
        int32_t click_max = 0;
        int16_t error_count = 0;
    }press_calib_para_set_t;

    press_calib_para_set_t para;    // 校准的参数设置

    ndt_sensor_cali();
    unsigned short *ndt_sensor_cali_process(int count, short *adc);
    void Sensor_cali_Init(CAL_CHANNEL_E machine);
    void sensor_cali_set(uint8_t stat);
    void sensor_test_status_set(TEST_STATUS_E t_status);
    void sensor_cali_reset();
    unsigned char Noise_CalProc(short *adc, short *noisePeak, short *noiseStd);
    char Calibration_Proces_ndt(short *adc, unsigned short *gRawDataFactor, short *err);
    unsigned char Self_Checking_Proces(short *adc, short *err);

    // state
    int32_t gs32SensorFlag = 0;
    short err[CHANNEL_MAX] = {0};               //错误码
    short test_result[CHANNEL_MAX] = {0};       //测试结果

    // calib_data
    int32_t first_adc[CHANNEL_MAX] = {0};
    int32_t brush_before_calib_count = 0;
    int button_before_calib_count[CHANNEL_MAX] = {0};

    // test_data   test-time  test-num  test-range  test-count
    TEST_STATUS_E test_status = TEST_INVALID;
    CALIB_STATUS_E calib_status = CALIB_INVALID;

    // text_output
    QString ui_msg_tip[3];
    QString ui_msg_err[3];
    QString ui_msg_test[3];

    // send_state
    int32_t send_state = 0;
private:
    typedef enum
    {
        cal_no_flag = 0,
        cal_self_checking_flag,
        cal_ch0_press_flag,
        cal_ch0_leave_flag,
        cal_ch1_press_flag,
        cal_ch1_leave_flag,
        cal_finally_flag
    } CAL_FLAG_STATE;

    QString donotmove = "人员：别移动牙刷";

    uint16_t CAL_CHANNEL_NUM = 0;       // 校准通道数
    
    uint16_t CAL_SIGNAL_CH0 = 0;      // 差异值刷头
    uint16_t CAL_SIGNAL_CH1 = 0;       // 差异值按键

    uint16_t CAL_WEIGHT_CH0 = 0;      // 刷头校准质量(g)
    uint16_t CAL_WEIGHT_CH1 = 0;      // 按键校准质量(g)

    int brush_test_time = 600;
    int botton_test_time = 600;
    int MANUAL_CALIB_SET = 200;
    uint8_t gu8CaliReset = 0;   // 校准确认位
    short gSensorPressAdc[CHANNEL_MAX] = {0};
    int tempFactor;
    short adc[CHANNEL_MAX] = {0};

    unsigned short rawDataFactor[CHANNEL_MAX] = {0};
    short gNoiseDetectCount[CHANNEL_MAX] = {0};
    short iDiffSquSum[CHANNEL_MAX] = {0};
    short cycles = 0;
    short diffMaxFlag[CHANNEL_MAX] = {0};
    short noiseMax[CHANNEL_MAX] = {0};
    short adcHead[CHANNEL_MAX] = {0};
    char singleFlag[CHANNEL_MAX] = {0};
    char calProcessFlag = 0;

    int adcHeadSum[CHANNEL_MAX] = {0};
    short diffData[CHANNEL_MAX] = {0};
    int diffDataSum[CHANNEL_MAX] = {0};
    short diffDataCal[CHANNEL_MAX] = {0};
    short noisePeak[CHANNEL_MAX] = {0};
    short noiseStd[CHANNEL_MAX] = {0};
    short noiseLoopTime = 0;
    int noiseAdcBuf[CHANNEL_MAX][NOISE_BUF_COUNT] = {0};
    int noiseAdcBufSum[CHANNEL_MAX] = {0};
    int noiseAdcDiffSum[CHANNEL_MAX] = {0};
};

#endif   // sensor_cali_H
