#ifndef __PB_COMMON_PART_CODEC_H__
#define __PB_COMMON_PART_CODEC_H__
#include "ble_protocol_msg_process.h"
#include "server_data.pb.h"

// ble cmd
int pb_packet_connect_pro(connect_pro_rsp_st *in, ConnectPro *pb_out);
int pb_packet_algo_remind_data(remind_data_t *in, AlgorithmRemind *pb_out);

//rota
int pb_packet_rotas_data_rsp(rotas_data_rsp_t *in, RotasDataRsp *pb_out);
int pb_packet_rotas_result_req(rotas_result_req_t *in, RotasResultReq *pb_out);
int pb_packet_rotas_file_status_rsp(rotas_file_status_rsp_t *in, RotasFileStatusRsp *pb_out);

//iot
int pb_packet_ota_update_info(ota_update_info_t *in, OtaUpdateInfo *pb_out);
int pb_packet_file_info(iot_file_info_t *in, FileInfo *pb_out);

#endif

