

#include "imu_calibrate.h"
#include "qdebug.h"

#if _MSC_VER >= 1600
#pragma execution_character_set(push, "utf-8")
#endif

void imu_calibrate::imu_calib_init(void) {
    QString imureason = "";
    index = 0;
    suc_flg = 0;
    memset(raw_imu_data, 0, sizeof(raw_imu_data));
    memset(&calData, 0, sizeof(calData));
    memset(&imu_mean, 0, sizeof(imu_mean));
    return;
}

void imu_calibrate::imu_calib_print_orginal_data(void) {
    for (int i = 0; i < DATA_COUNT; i++) {
        printf("[%d] acc: %d, %d, %d, gyro: %d, %d, %d", i, raw_imu_data[i].acc[0],
               raw_imu_data[i].acc[1], raw_imu_data[i].acc[2], raw_imu_data[i].gyro[0],
               raw_imu_data[i].gyro[1], raw_imu_data[i].gyro[2]);
    }
}

// axes order: +X -X +Y -Y +Z -Z
short imu_calibrate::imu_calib_proc(ImuDataT* imu_in) {
    // emit
    // send_imu_cali_msg("accx:"+QString::number(imu_in->acc[0])+",accy:"+QString::number(imu_in->acc[1])+",accz:"+QString::number(imu_in->acc[2])+",gyrox:"+QString::number(imu_in->gyro[0])+",gyroy:"+QString::number(imu_in->gyro[1])+",gyroz:"+QString::number(imu_in->gyro[2]));
    // emit send_imu_data_to_csv(
    //     imu_time, QString::number(imu_in->gyro[0]) + "," + QString::number(imu_in->gyro[1]) + "," +
    //                   QString::number(imu_in->gyro[2]) + "," + QString::number(imu_in->acc[0]) +
    //                   "," + QString::number(imu_in->acc[1]) + "," +
    //                   QString::number(imu_in->acc[2]));

    index = (index + 1) % DATA_SLIDE;
    ARRAY_PUSH_BACK(raw_imu_data, imu_in);

    if (index != 0) {
        // emit send_imu_cali_msg("数据量不够");
        return -1;
    }

    // imu_calib_print_orginal_data();

    short ret;
    ret = imu_calib_search_valid_data();
    qDebug() << "imu_calib_search_valid_data" << ret;
    if (ret < 0) {
        //  emit
        //  send_imu_cali_msg("校准返回值为-2且imu_calib_search_valid_data返回值为"+QString::number(ret));
        return -2;
    }

    short start = DATA_SLIDE, end = DATA_COUNT;
    imu_mean.acc[0] = imu_calib_get_acc_mean(start, end, 0);
    imu_mean.acc[1] = imu_calib_get_acc_mean(start, end, 1);
    imu_mean.acc[2] = imu_calib_get_acc_mean(start, end, 2);
    imu_mean.gyro[0] = imu_calib_get_gyro_mean(start, end, 0);
    imu_mean.gyro[1] = imu_calib_get_gyro_mean(start, end, 1);
    imu_mean.gyro[2] = imu_calib_get_gyro_mean(start, end, 2);
    //    //vTaskDelay(10);
    printf("imu_mean acc: %d, %d, %d, gyro: %d, %d, %d", imu_mean.acc[0], imu_mean.acc[1],
           imu_mean.acc[2], imu_mean.gyro[0], imu_mean.gyro[1], imu_mean.gyro[2]);

    ret = imu_calib_imu_var_check(start, end); // 震动
    qDebug() << "imu_calib_imu_var_check" << ret;
    if (ret < 0) {
        //  emit
        //  send_imu_cali_msg("校准返回值为-3且imu_calib_imu_var_check返回值为"+QString::number(ret));
        return -3;
    }

    qDebug() << "mean acc: %d %d %d, gyro: %d, %d, %d" << imu_mean.acc[0] << imu_mean.acc[1]
             << imu_mean.acc[2] << imu_mean.gyro[0] << imu_mean.gyro[1] << imu_mean.gyro[2];
    emit send_imu_cali_msg("陀螺仪校准成功");
    imu_calib_gyro_bias_calib(); // 写入校准结果
    suc_flg = 1;
    return 0;
}

short imu_calibrate::imu_calib_search_valid_data(void) {
    ImuDataT max_val, min_val;
    short i, j;
    for (i = 0; i < 3; i++) {
        max_val.acc[i] = -10000;
        max_val.gyro[i] = -10000;
        min_val.acc[i] = 10000;
        min_val.gyro[i] = 10000;
    }

    // float acc_norm;
    // for (i = 0; i < DATA_COUNT; i++)
    //{
    //     acc_norm = sqrtf(
    //                    raw_imu_data[i].acc[0] * raw_imu_data[i].acc[0] +
    //                    raw_imu_data[i].acc[1] * raw_imu_data[i].acc[1] +
    //                    raw_imu_data[i].acc[2] * raw_imu_data[i].acc[2]);

    //    emit send_imu_data_to_csv(imu_time,",,,,,,"+QString::number(acc_norm));

    //    if (acc_norm < LOWER_VAL || acc_norm > UPPER_VAL)
    //    {
    //        if(acc_norm!=0){
    //        qDebug()<<"acc_norm="<<acc_norm;
    //        emit
    //        send_imu_cali_msg("raw_imu_data[i].acc[0]="+QString::number(raw_imu_data[i].acc[0]));
    //        emit
    //        send_imu_cali_msg("raw_imu_data[i].acc[1]="+QString::number(raw_imu_data[i].acc[1]));
    //        emit
    //        send_imu_cali_msg("raw_imu_data[i].acc[2]="+QString::number(raw_imu_data[i].acc[2]));
    //        emit send_imu_cali_msg("acc_norm="+QString::number(acc_norm));
    //        if(acc_norm > UPPER_VAL){
    //            imureason="acc_normol="+QString::number(acc_norm);
    //            emit
    //            send_imu_cali_reslt_msg("acc_norm="+QString::number(acc_norm)+"大于阈值"+QString::number(UPPER_VAL));
    //        }
    //        if(acc_norm < LOWER_VAL){
    //            imureason="acc_normol="+QString::number(acc_norm);
    //            emit
    //            send_imu_cali_reslt_msg("acc_norm="+QString::number(acc_norm)+"小于阈值"+QString::number(LOWER_VAL));
    //        }

    //        }

    //        return -1;
    //    }
    //}
    // emit send_imu_cali_reslt_msg("acc_norm="+QString::number(acc_norm)+"合格");

    for (i = 0; i < DATA_COUNT; i++) {
        for (j = 0; j < AXES_NUM; j++) {
            if (raw_imu_data[i].acc[j] > max_val.acc[j]) {
                max_val.acc[j] = raw_imu_data[i].acc[j];
            }
            if (raw_imu_data[i].acc[j] < min_val.acc[j]) {
                min_val.acc[j] = raw_imu_data[i].acc[j];
            }
            if (raw_imu_data[i].gyro[j] > max_val.gyro[j]) {
                max_val.gyro[j] = raw_imu_data[i].gyro[j];
            }
            if (raw_imu_data[i].gyro[j] < min_val.gyro[j]) {
                min_val.gyro[j] = raw_imu_data[i].gyro[j];
            }
        }
    }

    emit send_imu_data_to_csv(imu_time,
                              ",,,,,,," + QString::number(max_val.acc[0] - min_val.acc[0]) + "," +
                                  QString::number(max_val.acc[1] - min_val.acc[1]) + "," +
                                  QString::number(max_val.acc[2] - min_val.acc[2]) + "," +
                                  QString::number(max_val.gyro[0] - min_val.gyro[0]) + "," +
                                  QString::number(max_val.gyro[1] - min_val.gyro[1]) + "," +
                                  QString::number(max_val.gyro[2] - min_val.gyro[2]));
    for (j = 0; j < AXES_NUM; j++) {
        if (max_val.acc[j] - min_val.acc[j] > IMU_RANGE_THRES) {
            emit send_imu_cali_reslt_msg(
                "acc差值=" + QString::number(max_val.acc[j] - min_val.acc[j]) + "大于阈值" +
                QString::number(IMU_RANGE_THRES));
            imureason = "acc差值=" + QString::number(max_val.acc[j] - min_val.acc[j]);
            emit send_imu_cali_msg("(max_val.acc[j] - min_val.acc[j])=" +
                                   QString::number(max_val.acc[j] - min_val.acc[j]));
            return -2;
        }
        if (max_val.gyro[j] - min_val.gyro[j] > IMU_RANGE_THRES) {
            emit send_imu_cali_reslt_msg(
                "gyro差值=" + QString::number(max_val.gyro[j] - min_val.gyro[j]) + "大于阈值" +
                QString::number(IMU_RANGE_THRES));
            imureason = "gyro差值=" + QString::number(max_val.gyro[j] - min_val.gyro[j]);
            emit send_imu_cali_msg("(max_val.gyro[j] - min_val.gyro[j])=" +
                                   QString::number(max_val.gyro[j] - min_val.gyro[j]));
            return -3;
        }
    }

    return 0;
}

int imu_calibrate::imu_calib_get_acc_mean(short start, short end, short axes) {
    int mean_tmp = 0;
    for (short i = start; i < end; i++) {
        mean_tmp += raw_imu_data[i].acc[axes];
    }
    mean_tmp /= (end - start);
    return mean_tmp;
}

int imu_calibrate::imu_calib_get_gyro_mean(short start, short end, short axes) {
    int mean_tmp = 0;
    for (short i = start; i < end; i++) {
        mean_tmp += raw_imu_data[i].gyro[axes];
    }
    mean_tmp /= (end - start);
    return mean_tmp;
}

short imu_calibrate::imu_calib_imu_var_check(short start, short end) {
    float acc_var[3] = {0.f};
    float gyro_var[3] = {0.f};
    for (short i = 0; i < 3; i++) {
        for (short j = start; j < end; j++) {
            acc_var[i] += (raw_imu_data[j].acc[i] - imu_mean.acc[i]) *
                (raw_imu_data[j].acc[i] - imu_mean.acc[i]);
            gyro_var[i] += (raw_imu_data[j].gyro[i] - imu_mean.gyro[i]) *
                (raw_imu_data[j].gyro[i] - imu_mean.gyro[i]);
        }
        acc_var[i] /= (end - start);
        gyro_var[i] /= (end - start);
    }

    emit send_imu_data_to_csv(
        imu_time, ",,,,,,,,,,,,," + QString::number(gyro_var[0]) + "," + QString::number(gyro_var[1]) + "," + QString::number(gyro_var[2]) + "," + QString::number(acc_var[0]) + "," + QString::number(acc_var[1]) + "," + QString::number(acc_var[2]));
    qDebug() << "acc_var: 和 gyro_var:" << acc_var[0] << acc_var[1] << acc_var[2] << gyro_var[0]
             << gyro_var[1] << gyro_var[2];
    // #define ACC_VAR_THRES   270
    // #define GYRO_VAR_THRES  10
    if ((gyro_var[0] > GYRO_VAR_THRES || gyro_var[1] > GYRO_VAR_THRES ||
         gyro_var[2] > GYRO_VAR_THRES) ||
        (acc_var[0] > ACC_VAR_THRES || acc_var[1] > ACC_VAR_THRES || acc_var[2] > ACC_VAR_THRES) ||
        ((gyro_var[0] == 0) && (gyro_var[1] == 0) && (gyro_var[2] == 0)) ||
        ((acc_var[0] == 0) && (acc_var[1] == 0) && (acc_var[2] == 0))) {
        emit send_imu_cali_msg("gyro_var0为:" + QString::number(gyro_var[0]) +
                               ",gyro_var1为:" + QString::number(gyro_var[1]) +
                               ",gyro_var2为:" + QString::number(gyro_var[2]));
        emit send_imu_cali_msg("acc_var0为:" + QString::number(acc_var[0]) +
                               ",acc_var1为:" + QString::number(acc_var[1]) +
                               ",acc_var2为:" + QString::number(acc_var[2]));
        emit send_imu_cali_msg("");
        imureason = "方差比对失败:gyro_var0为:" + QString::number(gyro_var[0]) +
            ",gyro_var1为:" + QString::number(gyro_var[1]) +
            ",gyro_var2为:" + QString::number(gyro_var[2]) +
            "acc_var0为:" + QString::number(acc_var[0]) +
            ",acc_var1为:" + QString::number(acc_var[1]) +
            ",acc_var2为:" + QString::number(acc_var[2]);
        emit send_imu_cali_reslt_msg("gyro_var0为:" + QString::number(gyro_var[0]) +
                                     ",gyro_var1为:" + QString::number(gyro_var[1]) +
                                     ",gyro_var2为:" + QString::number(gyro_var[2]));
        emit send_imu_cali_reslt_msg("acc_var0为:" + QString::number(acc_var[0]) +
                                     ",acc_var1为:" + QString::number(acc_var[1]) +
                                     ",acc_var2为:" + QString::number(acc_var[2]));
        emit send_imu_cali_reslt_msg("方差比对失败");

        return -1;
    }
    return 0;
}

// float get_no_bias_acc_norm(short calib_axes, ImuDataT* imu_mean_tmp) {
//     float no_bias_acc_pos[3] = {
//         imu_mean_tmp->acc[0] - calData.imu_offset.acc[0],
//         imu_mean_tmp->acc[1] - calData.imu_offset.acc[1],
//         imu_mean_tmp->acc[2] - calData.imu_offset.acc[2]
//     };
//
//     return sqrtf(no_bias_acc_pos[0] * no_bias_acc_pos[0] +
//         no_bias_acc_pos[1] * no_bias_acc_pos[1] +
//         no_bias_acc_pos[2] * no_bias_acc_pos[2]);
// }

void imu_calibrate::imu_calib_gyro_bias_calib(void) {
    for (short i = 0; i < AXES_NUM; i++) {
        calData.gyro_offset[i] = imu_mean.gyro[i];
    }
    emit send_imu_cali_msg(QString("calData.gyro_offset[0]=") +
                           QString::number(calData.gyro_offset[0]) +
                           ", calData.gyro_offset[1]=" + QString::number(calData.gyro_offset[1]) +
                           ", calData.gyro_offset[2]=" + QString::number(calData.gyro_offset[2]));

    imu_calib_write_calData();
    return;
}

/*保存结果到flash*/
short imu_calibrate::imu_calib_write_calData(void) {
    calData.magic = IMU_CAL_DATA_MAGIC;
    // hal_flash_erase(ADDR_GSENSOR_FLAG, sizeof(calData));
    // vTaskDelay(10);
    // hal_flash_write(ADDR_GSENSOR_FLAG, (uint8_t *)&calData, sizeof(calData));

    qDebug() << "校准结果：gyro:" << calData.gyro_offset[0] << calData.gyro_offset[1]
             << calData.gyro_offset[2];
    emit send_imu_data_to_csv(imu_time, ",,,,,,,,,,,,,,,,,,," + QString::number(calData.gyro_offset[0]) + "," + QString::number(calData.gyro_offset[1]) + "," + QString::number(calData.gyro_offset[2]));

    return 0;
}

/*从flash获取校准值*/
short imu_calibrate::imu_calib_read_calData(void) {
    // hal_flash_read(ADDR_GSENSOR_FLAG, (uint8_t *)&calData, sizeof(calData));

    if (calData.magic != IMU_CAL_DATA_MAGIC) {
        ImuCalDataOld old;
        if (calData.magic == IMU_CAL_DATA_MAGIC_OLD) {
            //  hal_flash_read(ADDR_GSENSOR_FLAG, (uint8_t *)&old, sizeof(old));

            printf("old calib valud. gyro: %d, %d, %d", old.imu_offset.gyro[0],
                   old.imu_offset.gyro[1], old.imu_offset.gyro[2]);

            memcpy(calData.gyro_offset, old.imu_offset.gyro, sizeof(calData.gyro_offset));
            imu_calib_write_calData();
        } else {
            memset(&calData, 0, sizeof(calData));
        }
    }

    printf("gyro: %d, %d, %d", calData.gyro_offset[0], calData.gyro_offset[1],
           calData.gyro_offset[2]);

    return 0;
}

ImuCalData* imu_calibrate::imu_calib_calData_get(void) {
    return &calData;
}

int imu_calibrate::imu_calib_is_suc(void) {
    return suc_flg;
}
