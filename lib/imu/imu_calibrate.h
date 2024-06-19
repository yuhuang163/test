#ifndef __IMU_CALIBRATE_H__
#define __IMU_CALIBRATE_H__

#include "Abini.h"
#define DATA_COUNT             100
#define DATA_SLIDE             25
#define AXES_NUM               3
// #define MIN_DATA_LEN    50
// #define UPPER_VAL       8600
// #define LOWER_VAL       7800
// #define AIM_VAL         8192
#define ACC_VAR_THRES          270
#define GYRO_VAR_THRES         10
#define IMU_RANGE_THRES        300
// #define NON_CALIB_VAL   450
#define IMU_CAL_DATA_MAGIC_OLD 0xA5A5A5A5
#define IMU_CAL_DATA_MAGIC     0x5A5A5A5A

#define ARRAY_PUSH_BACK(arr, data)                                              \
    {                                                                           \
        memmove(arr, (arr) + 1, sizeof(arr) - sizeof(arr[0]));                  \
        memcpy(&(arr[sizeof(arr) / sizeof(arr[0]) - 1]), data, sizeof(arr[0])); \
    }

class imu_calibrate : public QObject
{
    Q_OBJECT
public:
    void imu_calib_init(void);
    short imu_calib_proc(ImuDataT *imu_in);
    short imu_calib_get_org_data(ImuDataT **ptr);
    short imu_calib_find_calibed_axes(short *axes);
    short imu_calib_write_calData(void);
    short imu_calib_read_calData(void);
    ImuCalData *imu_calib_calData_get(void);
    int imu_calib_is_suc(void);
    int imu_calib_get_gyro_mean(short start, short end, short axes);
    int imu_calib_get_acc_mean(short start, short end, short axes);
    short imu_calib_imu_var_check(short start, short end);
    short imu_calib_search_valid_data(void);
    void imu_calib_gyro_bias_calib(void);
    void imu_calib_print_orginal_data(void);
    ImuCalData calData;
    QString imureason = "";
    QString imu_time;

private:
    ImuDataT raw_imu_data[DATA_COUNT] = {0};
    ImuDataT imu_mean = {{0}};
    short index = 0;
    int suc_flg = 0;

signals:
    void send_imu_cali_msg(QString msg);
    void send_imu_cali_reslt_msg(QString msg);
    void send_imu_data_to_csv(QString time, QString msg);
    
};

#endif
