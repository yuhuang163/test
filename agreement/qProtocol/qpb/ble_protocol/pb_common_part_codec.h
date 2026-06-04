#ifndef __PB_COMMON_PART_CODEC_H__
#define __PB_COMMON_PART_CODEC_H__
#include "ble_protocol_msg_process.h"
#include "server_data.pb.h"

// ble cmd
int pb_packet_connect_pro(connect_pro_rsp_st *in, ConnectPro *pb_out);
int pb_packet_algo_remind_data(remind_data_t *in, AlgorithmRemind *pb_out);
int pb_packet_upload_brush_report(void *context, brush_report_info_t *in, BrushingReport *pb_out);
int pb_packet_device_state(device_state_t *in, DeviceState *pb_out);
int pb_packet_audio_info(audio_data_info_t *in, AudioDataInfo *pb_out);
int pb_packet_upload_sensor_data(data_burial_point_t *in, SensorData *pb_out);

#if LED_LIGHTING_CUSTOM
int pb_packet_light_custom_info(light_custom_info_t *in, LightCustomInfo *pb_out);
#endif

//rota
int pb_packet_rotas_data_rsp(rotas_data_rsp_t *in, RotasDataRsp *pb_out);
int pb_packet_rotas_result_req(rotas_result_req_t *in, RotasResultReq *pb_out);
int pb_packet_rotas_file_status_rsp(rotas_file_status_rsp_t *in, RotasFileStatusRsp *pb_out);
int pb_packet_rotas_result_rsp(rotas_data_rsp_t *in, RotasResultRsp *pb_out);

//iot
int pb_packet_ota_update_info(ota_update_info_t *in, OtaUpdateInfo *pb_out);
int pb_packet_file_info(iot_file_info_t *in, FileInfo *pb_out);

int pb_packet_display_setting_info(display_setting_info_t *in, DisplaySettingInfo *pb_out);
int pb_packet_audio_remind_state(audio_remind_data_t *in, AudioRemindData *pb_out);

#endif

