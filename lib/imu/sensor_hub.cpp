#include "sensor_hub.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

// osThreadId acccalib_thread_id = NULL;
// TimerHandle_t sensorhub_timer = NULL;
#if _MSC_VER >= 1600
    #pragma execution_character_set("utf-8")
#endif
void free_int_3(new_imu_calibrate::sensordata_q *q)
{
    free(q->data);
    free(q);
}

void enqueue_int_3(new_imu_calibrate::sensordata_q *q, int value[3])
{
    if ((q->tail + 1) % q->capacity == q->head)
    {
        q->head = (q->head + 1) % q->capacity;
    }
    memcpy(q->data[q->tail], value, sizeof(int) * 3);
    q->tail = (q->tail + 1) % q->capacity;
}

int q_convolution_int_3(new_imu_calibrate::sensordata_q *q, int kernel[8])
{
    int conv[3] = {0, 0, 0};
    int i, j;
    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 8; j++)
        {
            conv[i] += q->data[(q->head + j) % q->capacity][i] * kernel[j];
        }
        if (conv[i] < 0)
            conv[i] = 0 - conv[i];
    }
    return (int)((conv[0] + conv[1] + conv[2]) / 3);
}

new_imu_calibrate::sensordata_q *new_imu_calibrate::init_int_3(int n)
{
    sensordata_q *q = (sensordata_q *)malloc(sizeof(sensordata_q));

    q->data = (int(*)[3])malloc(sizeof(int[3]) * n);
    q->head = 0;
    q->tail = 0;
    q->capacity = n;
    q->static_conv_count = 0;
    q->static_data_count = 0;
    q->mean_cal_done = false;
    for (int i = 0; i < 3; i++)
    {
        q->accdata_mean[i] = 0;
        q->accdata_sum[i] = 0;
    }
    q->enqueue = enqueue_int_3;
    q->free = free_int_3;
    q->static_detector = q_convolution_int_3;
    return q;
}

bool new_imu_calibrate::add_CalibData(float *data, CalibDatasets *add_calib_data)
{
    double fixed_positions[11][3] = {};   // 去掉const关键字

    if (LSB == 4)
    {

        if(imu_static_state==1){
        // for y20
        // 根据条件填充数组
        double y20_positions[11][3] = {
            {0, 0, 1},             // Position 8     u     u
            {0, 1, 0},             // Position 9		r
            {0, 0, -1},            // Position 10    d
            {-0.643, 0, 0.766},    // Position 6 	d     u
            {-0.643, 0.766, 0},    // Position 4 	d     l
            {-0.643, 0, -0.766},   // Position 7 	d     d
            {-0.643, -0.766, 0},   // Position 5 	d     r
            {0.643, 0, 0.766},     // Position 2 	u     u
            {0.643, -0.766, 0},    // Position 1 	u     l
            {0.643, 0, -0.766},     // Position 3 	u     d
            {0.643, 0.766, 0}     // Position 0 	u     r


        };
        memcpy(fixed_positions, y20_positions, sizeof(fixed_positions));

        }else{
                double q20_positions[11][3] = {

                {0, 0, 1},             // Position 1
                {1, 0, 0},             // Position 2
                {0, 0, -1} ,           // Position 3
                {0, 0.643, 0.766},     // Position 4
                {0.766, 0.643, 0},     // Position 5
                {0, 0.643, -0.766},    // Position 6
                {-0.766, 0.643, 0},    // Position 7
                {0, -0.643, 0.766},    // Position 8
                {0.766, -0.643, 0},    // Position 9
                {0, -0.643, -0.766},    // Position 10
                {-0.766, -0.643, 0}   // Position 11

                };
            memcpy(fixed_positions, q20_positions, sizeof(fixed_positions));

        }
    }
    else
    {
        // for q20 and p20pro
        double q20_positions[11][3] = {
            {0, 0, 1},             // Position 1
            {1, 0, 0},             // Position 2
            {0, 0, -1} ,           // Position 3
            {0, 0.643, 0.766},     // Position 4
            {0.766, 0.643, 0},     // Position 5
            {0, 0.643, -0.766},    // Position 6
            {-0.766, 0.643, 0},    // Position 7
            {0, -0.643, 0.766},    // Position 8
            {0.766, -0.643, 0},    // Position 9
            {0, -0.643, -0.766},    // Position 10
            {-0.766, -0.643, 0}   // Position 11

        };
        memcpy(fixed_positions, q20_positions, sizeof(fixed_positions));
    }

    for (int i = 0; i < 11; i++)
    {
        if (fabs(data[0] - fixed_positions[i][0]) < MAX_POSE_ERROR &&
            fabs(data[1] - fixed_positions[i][1]) < MAX_POSE_ERROR &&
            fabs(data[2] - fixed_positions[i][2]) < MAX_POSE_ERROR)
        {
            for (int k = 0; k < calib_datasets->size; k++)
            {
                if (fabs(calib_datasets->data[k].data[0] - fixed_positions[i][0]) < MAX_POSE_ERROR &&
                    fabs(calib_datasets->data[k].data[1] - fixed_positions[i][1]) < MAX_POSE_ERROR &&
                    fabs(calib_datasets->data[k].data[2] - fixed_positions[i][2]) < MAX_POSE_ERROR)
                {
                    emit send_imu_cali_msg("位置已经存在,换一个");
                    return false;
                }
            }

            add_calib_data->data[add_calib_data->size].data[0] = data[0];
            add_calib_data->data[add_calib_data->size].data[1] = data[1];
            add_calib_data->data[add_calib_data->size].data[2] = data[2];

            printf("data %d added: %.2f, %.2f, %.2f\n", add_calib_data->size,
                   add_calib_data->data[add_calib_data->size].data[0],
                   add_calib_data->data[add_calib_data->size].data[1],
                   add_calib_data->data[add_calib_data->size].data[2]);

            emit send_imu_cali_msg("data:" + QString::number(add_calib_data->size) + +",added" +
                                   QString::number(add_calib_data->data[add_calib_data->size].data[0]) +
                                   QString::number(add_calib_data->data[add_calib_data->size].data[1]) +
                                   QString::number(add_calib_data->data[add_calib_data->size].data[2]));
            add_calib_data->size++;
            qDebug() << "状态加1"
                     << "位置为" << i;

            emit send_imu_cali_position(i);
            emit send_imu_cali_msg("x位置为:" + QString::number(fixed_positions[i][0]) +
                                   ",y位置为:" + QString::number(fixed_positions[i][1]) +
                                   ",z位置为:" + QString::number(fixed_positions[i][2]));

            return true;
        }
    }
    emit send_imu_cali_msg("位置不对,换一个");

    emit send_imu_cali_msg("data:" + QString::number(add_calib_data->size) + +",added" +
                           QString::number(add_calib_data->data[add_calib_data->size].data[0]) +
                           QString::number(add_calib_data->data[add_calib_data->size].data[1]) +
                           QString::number(add_calib_data->data[add_calib_data->size].data[2]));

    return false;
}

new_imu_calibrate::CalibDatasets *new_imu_calibrate::init_CalibData()
{
    calib_data = (CalibDatasets *)malloc(sizeof(CalibDatasets));

    for (int i = 0; i < FACTORY_POSE_NUM; i++)
    {
        for (int j = 0; j < 3; j++)
        {   // make sure the initial value is not near the fixed position
            calib_data->data[i].data[j] = 0;
        }
    }
    calib_data->size = 0;
    calib_data->add = &new_imu_calibrate::add_CalibData;

    return calib_data;
}
new_imu_calibrate::CalibDatasets *new_imu_calibrate::init_ExtraCalibData()
{
    extra_calib_data = (CalibDatasets *)malloc(sizeof(CalibDatasets));

    for (int i = 0; i < EXTRA_POSE_NUM; i++)
    {
        for (int j = 0; j < 3; j++)
        {   // make sure the initial value is not near the fixed position
             extra_calib_data->data[i].data[j] = 0;
        }
    }

    extra_calib_data->size = 0;
   // qDebug() << "指向完成";
    return extra_calib_data;
}

new_imu_calibrate::imu_calib_parameters *new_imu_calibrate::imu_calib_parameters_alloc()
{
    imu_calib_parameters *imu_calib = (imu_calib_parameters *)malloc(sizeof(imu_calib_parameters));
    imu_calib->acc_calib = us_matrix_float_alloc(9, 1);
    return imu_calib;
}

// update acc data with calib parameters.
// convert type here.
void new_imu_calibrate::acc_update(us_matrix_float *acc_data, CalibDatasets *acc_raw,
                                   us_matrix_float *theta)
{
    for (int i = 0; i < acc_raw->size; i++)
    {
        us_matrix_float_set(acc_data, i, 0,
                            THETA_ACC(0) * (acc_raw->data[i].data[0] - THETA_ACC(6)));
        us_matrix_float_set(acc_data, i, 1,
                            (0 - THETA_ACC(3)) * THETA_ACC(0) *
                                    (acc_raw->data[i].data[0] - THETA_ACC(6)) +
                                THETA_ACC(1) * (acc_raw->data[i].data[1] - THETA_ACC(7)));
        us_matrix_float_set(
            acc_data, i, 2,
            (0 - THETA_ACC(4)) * THETA_ACC(0) * (acc_raw->data[i].data[0] - THETA_ACC(6)) +
                (0 - THETA_ACC(5)) * THETA_ACC(1) * (acc_raw->data[i].data[1] - THETA_ACC(7)) +
                THETA_ACC(2) * (acc_raw->data[i].data[2] - THETA_ACC(8)));
    }
    return;
}

// calculate value for each point; meanwhile cal and return loss
float new_imu_calibrate::acccalib_vl(us_matrix_float *v, us_matrix_float *acc_data)
{
    float loss = 0;
    for (int i = 0; i < acc_data->size1; i++)
    {
        us_matrix_float_set(
            v, i, 0,
            1 - sqrt(us_matrix_float_get(acc_data, i, 0) * us_matrix_float_get(acc_data, i, 0) +
                     us_matrix_float_get(acc_data, i, 1) * us_matrix_float_get(acc_data, i, 1) +
                     us_matrix_float_get(acc_data, i, 2) * us_matrix_float_get(acc_data, i, 2)));
    }

    for (int i = 0; i < acc_data->size1; i++)
    {
        loss += fabs(us_matrix_float_get(v, i, 0));
    }

    return loss;
}

void new_imu_calibrate::acccalib_jacobian(us_matrix_float *J, CalibDatasets *acc_raw,
                                          us_matrix_float *theta)
{
    for (int i = 0; i < FACTORY_POSE_NUM; i++)
    {
        float ax = THETA_ACC(0) * (acc_raw->data[i].data[0] - THETA_ACC(6));
        float ay = (0 - THETA_ACC(3)) * THETA_ACC(0) * (acc_raw->data[i].data[0] - THETA_ACC(6)) +
                   THETA_ACC(1) * (acc_raw->data[i].data[1] - THETA_ACC(7));
        float az = (0 - THETA_ACC(4)) * THETA_ACC(0) * (acc_raw->data[i].data[0] - THETA_ACC(6)) +
                   (0 - THETA_ACC(5)) * THETA_ACC(1) * (acc_raw->data[i].data[1] - THETA_ACC(7)) +
                   THETA_ACC(2) * (acc_raw->data[i].data[2] - THETA_ACC(8));
        us_matrix_float_set(
            J, i, 0,
            -2 * (ax * (acc_raw->data[i].data[0] - THETA_ACC(6)) +
                  ay * (-1) * THETA_ACC(3) * (acc_raw->data[i].data[0] - THETA_ACC(6)) +
                  az * (-1) * THETA_ACC(4) * (acc_raw->data[i].data[0] - THETA_ACC(6))));
        us_matrix_float_set(
            J, i, 1,
            -2 * (ay * (acc_raw->data[i].data[1] - THETA_ACC(7)) +
                  az * (-1) * (THETA_ACC(5) * (acc_raw->data[i].data[1] - THETA_ACC(7)))));
        us_matrix_float_set(J, i, 2, -2 * (az * (acc_raw->data[i].data[2] - THETA_ACC(8))));
        us_matrix_float_set(
            J, i, 3, -2 * (ay * (-1) * (THETA_ACC(0)) * (acc_raw->data[i].data[0] - THETA_ACC(6))));
        us_matrix_float_set(
            J, i, 4, -2 * (az * (-1) * (THETA_ACC(0)) * (acc_raw->data[i].data[0] - THETA_ACC(6))));
        us_matrix_float_set(
            J, i, 5, -2 * (az * (-1) * (THETA_ACC(1)) * (acc_raw->data[i].data[1] - THETA_ACC(7))));
        us_matrix_float_set(J, i, 6,
                            -2 * (ax * (-1) * THETA_ACC(0) + ay * THETA_ACC(3) * THETA_ACC(0) +
                                  az * THETA_ACC(4) * THETA_ACC(0)));
        us_matrix_float_set(J, i, 7,
                            -2 * (ay * (-1) * THETA_ACC(1) + az * THETA_ACC(5) * THETA_ACC(1)));
        us_matrix_float_set(J, i, 8, -2 * (az * (-1) * THETA_ACC(2)));
    }
}

/*
 * this timer collects data from sensor. and calculate the convolution
 * for Q10, only accerlation data is collected
 */
void new_imu_calibrate::sensorhub_timer_callback(ImuDataT *imu_in)
{
    int accdata[3];
    // qDebug() << "收到数据" ;
    int static_conv_kernel[8] = {-1, 3, -6, 9, -9, 6, -3, 1};
    bool ifstatic = false;

    // qma6100_read_raw_xyz(accdata);
    accdata[0] = imu_in->acc[0] / LSB;
    accdata[1] = imu_in->acc[1] / LSB;
    accdata[2] = imu_in->acc[2] / LSB;

    q->enqueue(q, accdata);
    qconv = q->static_detector(q, static_conv_kernel);

    if (qconv < STATIC_CONV_VAR)
    {
       // emit send_imu_cali_msg("qconv value===" + QString::number(qconv));
       // qDebug()<<"qconv value:"<< qconv;//打印数值
        q->static_conv_count++;
        // sum+=data
        if ((q->static_conv_count >= STATIC_CONV_DELAY) && (q->static_conv_count < STATIC_CONV_COUNT)) //取后段数据进行累加
        {
            for (int i = 0; i < 3; i++)
            {
                q->accdata_sum[i] += accdata[i];
            }
        }
    }
    else
    {
        q->static_conv_count = 0;
        // sum
        for (int i = 0; i < 3; i++)
        {
            q->accdata_sum[i] = 0;
        }
        q->mean_cal_done = false;
    }

    if (q->static_conv_count >= STATIC_CONV_COUNT)
    {
        ifstatic = true;
        // sum valid, update mean
        if (!q->mean_cal_done)
        {
            for (int i = 0; i < 3; i++)
            {
                q->accdata_mean[i] = (float)(q->accdata_sum[i] / (STATIC_CONV_COUNT-STATIC_CONV_DELAY)) / 2048;//增加延时取数据
                //qDebug() << "q->accdata_mean[i]" << q->accdata_mean[i];
            }
            q->mean_cal_done = true;

            if (calib_datasets->size < FACTORY_POSE_NUM)
            {
                if (add_CalibData(q->accdata_mean, calib_datasets))
                {
                    for (int i = 0; i < calib_datasets->size; i++)
                    {
                        printf("data %d: %.2f, %.2f, %.2f\n", i, calib_datasets->data[i].data[0],
                               calib_datasets->data[i].data[1], calib_datasets->data[i].data[2]);
                    }
                }
            }
            else if (extra_calib_datasets->size < EXTRA_POSE_NUM)//加载extra_calib_datasets
            {

                if (add_CalibData(q->accdata_mean, extra_calib_datasets))
                {
                    for (int i = 0; i < extra_calib_datasets->size; i++)
                    {
                        printf("extra_data %d: %.2f, %.2f, %.2f\n", i, extra_calib_datasets->data[i].data[0],
                               extra_calib_datasets->data[i].data[1], extra_calib_datasets->data[i].data[2]);
                    }
                }
            }

            for (int i = 0; i < 3; i++)
            {
                q->accdata_sum[i] = 0;
            }
        }
        q->static_conv_count = STATIC_CONV_COUNT;
    }


    // for visuallization
    // printf("$%d %d %d %d %d
    // %d;",accdata[0],accdata_mean[0],accdata[1],accdata_mean[1],accdata[2],accdata_mean[2]);

    return;
}

void new_imu_calibrate::acccalib_sensors_init()
{
    QSettings settings(SETTING_NAME, QSettings::IniFormat);
    STATIC_CONV_VAR = settings.value("IMU/STATIC_CONV_VAR", "200").toInt();
    STATIC_CONV_COUNT = settings.value("IMU/STATIC_CONV_COUNT", "45").toInt();
    STATIC_CONV_DELAY= settings.value("IMU/STATIC_CONV_DELAY", "15").toInt();
    // init sensorhub timer. it's will collect data from sensor

    q = init_int_3(8);   // init a int[3] q with capacity 25 for sensor data

    acccalib_datasets_matrix = us_matrix_float_alloc(FACTORY_POSE_NUM, 3);
    acccalib_value_matrix = us_matrix_float_alloc(FACTORY_POSE_NUM, 1);
    acccalib_jacobian_matrix = us_matrix_float_alloc(FACTORY_POSE_NUM, 9);
    acccalib_gradient_matrix = us_matrix_float_alloc(9, 1);
    acccalib_delta_matrix = us_matrix_float_alloc(9, 1);
    imu_calib = imu_calib_parameters_alloc();
    jtj = us_matrix_float_alloc(9, 9);
    inv = us_matrix_float_alloc(9, 9);
    p = us_permutation_alloc(9);
    lambda_LM=100;
    // init imu_calib_parameters to [1,1,1,0,0,0,0,0,0]
    us_matrix_float_set(imu_calib->acc_calib, 0, 0, 1);
    us_matrix_float_set(imu_calib->acc_calib, 1, 0, 1);
    us_matrix_float_set(imu_calib->acc_calib, 2, 0, 1);
    us_matrix_float_set(imu_calib->acc_calib, 3, 0, 0);
    us_matrix_float_set(imu_calib->acc_calib, 4, 0, 0);
    us_matrix_float_set(imu_calib->acc_calib, 5, 0, 0);
    us_matrix_float_set(imu_calib->acc_calib, 6, 0, 0);
    us_matrix_float_set(imu_calib->acc_calib, 7, 0, 0);
    us_matrix_float_set(imu_calib->acc_calib, 8, 0, 0);
    loss=0;
    cali_loop =0;
    // if(temp_calib_datasets != nullptr)
    // {
    //     free(temp_calib_datasets);
    //  }
    calib_datasets = init_CalibData();//calib_data
    extra_calib_datasets = init_ExtraCalibData();//extra_calib_data


    printf("2:start acccalib task!\n");
}

// acccalib sensor task:
// only avaliable before first ship mode
// record acc data and detecte static intervals(in timer)
// using more than 9 intervals to calibrate acc calib parameters
// after gathering enough data, run LM to get acc calib parameters
int new_imu_calibrate::acccalib_sensors_task()
{
   // qDebug() << "calib_datasets->size：" <<calib_datasets->size;
   // qDebug() << "extra_calib_datasets->size：" <<extra_calib_datasets->size;

   // qDebug() << "初始loss等于" << loss;
    if ((calib_datasets->size >= FACTORY_POSE_NUM)&&(extra_calib_datasets->size >= EXTRA_POSE_NUM))
    {
        if (cali_loop == 0)
        {

            temp_calib_datasets = (CalibDatasets *)malloc(sizeof(CalibDatasets));
            for (int i = 0; i < FACTORY_POSE_NUM; i++)
            {
                for (int j = 0; j < 3; j++)
                {
                    temp_calib_datasets->data[i].data[j] = calib_datasets->data[i].data[j];
                }
            }
        }

        for (int x = 0; x < 9; ++x)
        {
            qDebug() << "task:data" <<"x"<< calib_datasets->data[x].data[0] << calib_datasets->data[x].data[1] << calib_datasets->data[x].data[2];
        }
        qDebug() << "extra_datasets:" << extra_calib_datasets->data[0].data[0] << extra_calib_datasets->data[0].data[1] << extra_calib_datasets->data[0].data[2];

        // pre update. just for convert the data format
        // todo merge data struct
        acc_update(acccalib_datasets_matrix, calib_datasets, imu_calib->acc_calib);

        loss = acccalib_vl(acccalib_value_matrix, acccalib_datasets_matrix);
        printf("original loss = %f\n", loss);
        // LM iteration
        for (int x = 0; x < MAX_ITERATIONS; x++)
        {
            printf("---\n");
            printf("iter: %d\n", x);
            // update jacobian
            acccalib_jacobian(acccalib_jacobian_matrix, calib_datasets, imu_calib->acc_calib);

            // gradient = jacobian.T @ value(a)
            acc_update(acccalib_datasets_matrix, calib_datasets, imu_calib->acc_calib);
            us_blas_sgemm(CblasTrans, CblasNoTrans, 1.0, acccalib_jacobian_matrix,
                          acccalib_value_matrix, 0.0, acccalib_gradient_matrix);

            // delta=np.linalg.inv(jacobian.T@jacobian + lambda_*np.eye(9))@gradient
            us_blas_sgemm(CblasTrans, CblasNoTrans, 1.0, acccalib_jacobian_matrix,
                          acccalib_jacobian_matrix, 0.0, jtj);
            for (int i = 0; i < 9; i++)
            {
                us_matrix_float_set(jtj, i, i, us_matrix_float_get(jtj, i, i) + lambda_LM);
            }
            us_linalg_LU_decomp(jtj, p, &signum);
            us_linalg_LU_invert(jtj, p, inv);
            us_blas_sgemm(CblasNoTrans, CblasNoTrans, 1.0, inv, acccalib_gradient_matrix, 0.0,
                          acccalib_delta_matrix);

            // #update parameters
            // theta_acc = theta_acc - delta
            for (int i = 0; i < 9; i++)
            {
                us_matrix_float_set(imu_calib->acc_calib, i, 0,
                                    us_matrix_float_get(imu_calib->acc_calib, i, 0) -
                                        us_matrix_float_get(acccalib_delta_matrix, i, 0));
            }

            // update loss
            last_loss = loss;
            loss = acccalib_vl(acccalib_value_matrix, acccalib_datasets_matrix);

            qDebug() << "loss等于" << loss;
            emit send_imu_cali_msg("loss等于" + QString::number(loss));
            qDebug() << "lambda_LM原始" << lambda_LM;
            // addjust penalty term lambda
            if (loss < last_loss)
            {
                lambda_LM = lambda_LM * 0.5;
            }

            else
            {
                lambda_LM = lambda_LM * 2;
            }

            qDebug() << "lambda_LM计算后=" << lambda_LM;

            if (loss < ACC_CALIB_ACCURACY)
            {
                // 对imu_calib->acc_calib中的9个参数进行一个异常值限幅，0 1
                // 2限制到0.9~1.1或者-1.1~-0.9，3 4 5限制到-0.02~0.02，6 7
                // 8限制到-0.5~0.5。如果超出范围，认为校准失败，返回1。
                if ((us_matrix_float_get(imu_calib->acc_calib, 0, 0) > 1.3) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 0, 0) < 0.7 &&
                     us_matrix_float_get(imu_calib->acc_calib, 0, 0) > -0.7) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 0, 0) < -1.3) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 1, 0) > 1.3) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 1, 0) < 0.7 &&
                     us_matrix_float_get(imu_calib->acc_calib, 1, 0) > -0.7) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 1, 0) < -1.3) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 2, 0) > 1.3) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 2, 0) < 0.7 &&
                     us_matrix_float_get(imu_calib->acc_calib, 2, 0) > -0.7) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 2, 0) < -1.3) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 3, 0) > 0.1) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 3, 0) < -0.1) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 4, 0) > 0.1) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 4, 0) < -0.1) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 5, 0) > 0.1) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 5, 0) < -0.1) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 6, 0) > 0.5) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 6, 0) < -0.5) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 7, 0) > 0.5) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 7, 0) < -0.5) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 8, 0) > 0.5) ||
                    (us_matrix_float_get(imu_calib->acc_calib, 8, 0) < -0.5))
                {
                    if (cali_loop < FACTORY_POSE_NUM)
                    {
                        // 标定参数重新初始化
                        us_matrix_float_set(imu_calib->acc_calib, 0, 0, 1);
                        us_matrix_float_set(imu_calib->acc_calib, 1, 0, 1);
                        us_matrix_float_set(imu_calib->acc_calib, 2, 0, 1);
                        us_matrix_float_set(imu_calib->acc_calib, 3, 0, 0);
                        us_matrix_float_set(imu_calib->acc_calib, 4, 0, 0);
                        us_matrix_float_set(imu_calib->acc_calib, 5, 0, 0);
                        us_matrix_float_set(imu_calib->acc_calib, 6, 0, 0);
                        us_matrix_float_set(imu_calib->acc_calib, 7, 0, 0);
                        us_matrix_float_set(imu_calib->acc_calib, 8, 0, 0);
                        loss = 0;
                        if (cali_loop)
                        {
                            for (int i = 0; i < FACTORY_POSE_NUM; i++)
                            {
                                for (int j = 0; j < 3; j++)
                                {
                                    calib_datasets->data[i].data[j] = temp_calib_datasets->data[i].data[j];
                                }
                            }
                        }
                        //用extra_calib_datasets中的数据替换calib_datasets中的数据
                        calib_datasets->data[cali_loop].data[0] = extra_calib_datasets->data[0].data[0];
                        calib_datasets->data[cali_loop].data[1] = extra_calib_datasets->data[0].data[1];
                        calib_datasets->data[cali_loop].data[2] = extra_calib_datasets->data[0].data[2];
                        cali_loop++;
                        emit send_imu_cali_msg("重复次数：" + QString::number(cali_loop));
                        qDebug() << "重复次数：" << cali_loop;
                       int k= acccalib_sensors_task();
                        if(k == CALIB_SUCCESS)
                        {
                            return CALIB_SUCCESS;//重标校准成功
                        }
                    }

                    qDebug() << "数据异常，校准失败";
                    emit send_imu_cali_msg("数据异常，校准失败");
                    calData.kx = us_matrix_float_get(imu_calib->acc_calib, 0, 0);
                    calData.ky = us_matrix_float_get(imu_calib->acc_calib, 1, 0);
                    calData.kz = us_matrix_float_get(imu_calib->acc_calib, 2, 0);
                    calData.syx = us_matrix_float_get(imu_calib->acc_calib, 3, 0);
                    calData.szx = us_matrix_float_get(imu_calib->acc_calib, 4, 0);
                    calData.szy = us_matrix_float_get(imu_calib->acc_calib, 5, 0);
                    calData.bx = us_matrix_float_get(imu_calib->acc_calib, 6, 0);
                    calData.by = us_matrix_float_get(imu_calib->acc_calib, 7, 0);
                    calData.bz = us_matrix_float_get(imu_calib->acc_calib, 8, 0);
                    // 发射信号，将带有文本 "calData.kx=" 和 calData.kx 的值的字符串发送出去
                    emit send_imu_cali_msg("calData.kx=" + QString::number(calData.kx) +
                                           ", calData.ky=" + QString::number(calData.ky) +
                                           ", calData.kz=" + QString::number(calData.kz) +
                                           ", calData.syx=" + QString::number(calData.syx) +
                                           ", calData.szx=" + QString::number(calData.szx) +
                                           ", calData.szy=" + QString::number(calData.szy) +
                                           ", calData.bx=" + QString::number(calData.bx) +
                                           ", calData.by=" + QString::number(calData.by) +
                                           ", calData.bz=" + QString::number(calData.bz));

                     return CALIB_FAIL;// 校准失败
                }


                calData.kx = us_matrix_float_get(imu_calib->acc_calib, 0, 0);
                calData.ky = us_matrix_float_get(imu_calib->acc_calib, 1, 0);
                calData.kz = us_matrix_float_get(imu_calib->acc_calib, 2, 0);
                calData.syx = us_matrix_float_get(imu_calib->acc_calib, 3, 0);
                calData.szx = us_matrix_float_get(imu_calib->acc_calib, 4, 0);
                calData.szy = us_matrix_float_get(imu_calib->acc_calib, 5, 0);
                calData.bx = us_matrix_float_get(imu_calib->acc_calib, 6, 0);
                calData.by = us_matrix_float_get(imu_calib->acc_calib, 7, 0);
                calData.bz = us_matrix_float_get(imu_calib->acc_calib, 8, 0);

                emit send_imu_cali_msg("加速度校准成功");
                emit send_imu_cali_msg("calData.kx=" + QString::number(calData.kx) +
                                       ", calData.ky=" + QString::number(calData.ky) +
                                       ", calData.kz=" + QString::number(calData.kz) +
                                       ", calData.syx=" + QString::number(calData.syx) +
                                       ", calData.szx=" + QString::number(calData.szx) +
                                       ", calData.szy=" + QString::number(calData.szy) +
                                       ", calData.bx=" + QString::number(calData.bx) +
                                       ", calData.by=" + QString::number(calData.by) +
                                       ", calData.bz=" + QString::number(calData.bz));

                return CALIB_SUCCESS;   // 校准成功
            }
        }
        if (cali_loop < FACTORY_POSE_NUM)
        {
            //标定参数重新初始化
            us_matrix_float_set(imu_calib->acc_calib, 0, 0, 1);
            us_matrix_float_set(imu_calib->acc_calib, 1, 0, 1);
            us_matrix_float_set(imu_calib->acc_calib, 2, 0, 1);
            us_matrix_float_set(imu_calib->acc_calib, 3, 0, 0);
            us_matrix_float_set(imu_calib->acc_calib, 4, 0, 0);
            us_matrix_float_set(imu_calib->acc_calib, 5, 0, 0);
            us_matrix_float_set(imu_calib->acc_calib, 6, 0, 0);
            us_matrix_float_set(imu_calib->acc_calib, 7, 0, 0);
            us_matrix_float_set(imu_calib->acc_calib, 8, 0, 0);
            loss = 0;
            if (cali_loop)
            {
                for (int i = 0; i < FACTORY_POSE_NUM; i++)
                {
                    for (int j = 0; j < 3; j++)
                    {
                        calib_datasets->data[i].data[j] = temp_calib_datasets->data[i].data[j];
                    }
                }
            }
            calib_datasets->data[cali_loop].data[0] = extra_calib_datasets->data[0].data[0];
            calib_datasets->data[cali_loop].data[1] = extra_calib_datasets->data[0].data[1];
            calib_datasets->data[cali_loop].data[2] = extra_calib_datasets->data[0].data[2];
            cali_loop++;
            emit send_imu_cali_msg("重复次数：" + QString::number(cali_loop));
            qDebug() << "重复次数：" << cali_loop;
            int k= acccalib_sensors_task();
            if (k == CALIB_SUCCESS)
            {
                return CALIB_SUCCESS;//重标校准成功
            }
        }
        return CALIB_FAIL;   // 校准失败
    }

    return CALIB_NOT_START;   // 还没开始
}



void new_imu_calibrate::add_csvdata(CalibData*csv_data){



    while(calib_datasets->size<9){
    calib_datasets->data[calib_datasets->size].data[0] =  csv_data[calib_datasets->size].data[0];
    calib_datasets->data[calib_datasets->size].data[1] = csv_data[calib_datasets->size].data[1];
    calib_datasets->data[calib_datasets->size].data[2] = csv_data[calib_datasets->size].data[2];
    calib_datasets->size++;
}
    // for (int x = 0; x < 9; ++x) {
    //     qDebug() <<"task1data"<<csv_data[x].data[0]<< csv_data[x].data[1]<<csv_data[x].data[2];
    // }

    // for (int x = 0; x < 9; ++x) {
    //     qDebug() <<"task2data"<<calib_datasets->data[x].data[0]<< calib_datasets->data[x].data[1]<<calib_datasets->data[x].data[2];
    // }
}
