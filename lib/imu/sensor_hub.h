#ifndef __SENSOR_HUB_H__
#define __SENSOR_HUB_H__
#include "Abini.h"
#include "us_eigen_nonsymmsquare.h"

#ifdef USMILE_Q10
    #define SENSOR_CONFIG = A
#endif

#define SENSOR_DATA_SIZE 3
#define SENSOR_DATA_TYPE int

// add 2 config_file
// #define STATIC_CONV_VAR 200
// #define STATIC_CONV_COUNT 45

#define MAX_ITERATIONS     100
#define ACC_CALIB_ACCURACY 0.008
#define MAX_POSE_ERROR     0.3

#define ACC_CALIB_DATASIZE 12
#define FACTORY_POSE_NUM   9

#define THETA_ACC(n)       us_matrix_float_get(theta, n, 0)
#define DATA_ACC(n)        us_matrix_float_get(acc_data, n, 0)
typedef struct
{
    float data[3];
} CalibData;
class new_imu_calibrate : public QObject
{
    Q_OBJECT
signals:
    void send_imu_cali_msg(QString msg);
    void send_imu_cali_reslt_msg(QString msg);
    void send_imu_data_to_csv(QString time, QString msg);
    void send_imu_cali_position(int position);
public:

    int LSB = 8192;
    int imu_static_state = 1;

    QString imureason = "";
    QString imu_time;
    NewImuCalData calData;
    typedef struct sensordata_q
    {
        // sensor data queue
        SENSOR_DATA_TYPE (*data)[SENSOR_DATA_SIZE];
        int head;
        int tail;
        int capacity;

        // for static detection and mean calculation
        int static_conv_count;
        int static_data_count;
        bool mean_cal_done;
        int accdata_sum[3];
        float accdata_mean[3];

        SENSOR_DATA_TYPE (*sum)[SENSOR_DATA_SIZE];
        SENSOR_DATA_TYPE (*mean)[SENSOR_DATA_SIZE];
        SENSOR_DATA_TYPE (*var)[SENSOR_DATA_SIZE];
        void (*enqueue)(struct sensordata_q *, SENSOR_DATA_TYPE *);
        void (*free)(struct sensordata_q *);
        int (*static_detector)(struct sensordata_q *q, int kernel[8]);
    } sensordata_q;



    typedef struct CalibDatasets
    {
        CalibData data[FACTORY_POSE_NUM];
        int size;
        bool (new_imu_calibrate::*add)(float *data, struct CalibDatasets *calib_data);
    } CalibDatasets;

    typedef struct imu_calib_parameters
    {
        us_matrix_float *acc_calib;
        // todo: add gyro and mag calib struct
    } imu_calib_parameters;

    sensordata_q *init_int_3(int n);

    void acccalib_task_init(void);
    int acccalib_sensors_task();
    void sensorhub_timer_callback(ImuDataT *imu_in);
    void acccalib_sensors_init();

    void acccalib_jacobian(us_matrix_float *J, CalibDatasets *acc_raw, us_matrix_float *theta);

    CalibDatasets *init_CalibData();
    float acccalib_vl(us_matrix_float *v, us_matrix_float *acc_data);
    imu_calib_parameters *imu_calib_parameters_alloc();
    void acc_update(us_matrix_float *acc_data, CalibDatasets *acc_raw, us_matrix_float *theta);

    sensordata_q *q;
    CalibDatasets *calib_datasets;
    us_matrix_float *acccalib_datasets_matrix;
    us_matrix_float *acccalib_value_matrix;
    us_matrix_float *acccalib_jacobian_matrix;
    us_matrix_float *acccalib_gradient_matrix;
    us_matrix_float *acccalib_delta_matrix;
    imu_calib_parameters *imu_calib;
    float lambda_LM = 100;
    float loss = 0, last_loss = 0;
    int signum;
    us_matrix_float *jtj;
    us_matrix_float *inv;
    us_permutation *p;
    int  qconv = 0;

    bool add_CalibData(float *data, new_imu_calibrate::CalibDatasets *calib_data);
    CalibDatasets *calib_data;

    int STATIC_CONV_VAR = 200;
    int STATIC_CONV_COUNT = 45; 
    int STATIC_CONV_DELAY = 15; //延迟取数据
public:
    void add_csvdata( CalibData*csv_data);
};
#endif
