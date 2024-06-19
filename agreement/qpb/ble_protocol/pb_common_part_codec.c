#include "usmile_app_internal.h" 
#include "pb_common_part_codec.h"

int pb_packet_connect_pro(connect_pro_rsp_st *in, ConnectPro *pb_out)
{
    pb_out->version_code     = in->version_code;
    pb_out->model_id         = (ProductId)in->model_id;
    pb_out->mac_address.size = 6;
    memcpy(pb_out->mac_address.bytes, in->mac_address, 6);
    pb_out->protocol_id      = (ProtocolId)in->protocol_id;
    pb_out->firmware_type    = in->firmware_type;
    pb_out->firmware_id      = in->firmware_id;
    pb_out->hardware_id      = (HardwareId)in->hardware_id;
    pb_out->ui_res_id        = in->ui_res_id;
    pb_out->upgrade_type     = in->upgrade_type;
    pb_out->res_upgrade_type = in->res_upgrade_type;

    return 0;
}


int pb_packet_device_remind_data(remind_data_t *in, DeviceRemind *pb_out)
{
    return 0;
}

int pb_packet_algo_remind_data(remind_data_t *in, AlgorithmRemind *pb_out)
{
    pb_out->remind_type = (RemindType)in->remind_cmd;
    pb_out->remind_side = in->remind_side;
    switch(pb_out->remind_type)
    {
        case RemindType_OVER_PRESSURE: 
            pb_out->which_value = AlgorithmRemind_press_value_tag;
            pb_out->value.press_value = (RemindValue)in->remind_value;
            break;
        case RemindType_CHARGING_STATE:
            pb_out->which_value = AlgorithmRemind_charging_state_tag;
            pb_out->value.charging_state = (ChargingState)in->remind_value;
            break;
        default: break;
    }
    
    pb_out->remind_value = in->remind_count;
    
    LOG_PB("remind_type: %d, value: %d, count: %d", pb_out->remind_type, in->remind_value, in->remind_count);
    return 0;
}


int pb_packet_rotas_data_rsp(rotas_data_rsp_t *in, RotasDataRsp *pb_out)
{
    pb_out->fileType = in->fileType;
    pb_out->index = in->index;
    pb_out->result = in->result;
    pb_out->progress= in->progress;
    
    LOG_PB("fileType: %d, progress: %lu", pb_out->fileType, pb_out->progress);
    return 0;
}

int pb_packet_rotas_result_req(rotas_result_req_t *in, RotasResultReq *pb_out)
{
    pb_out->fileType = (RotasUpdateFile)in->fileType;
    pb_out->fileVersion = in->fileVersion;
    pb_out->rotaResult = (RotaErrorCode)in->rotaResult;
    
    LOG_PB("fileType: %d, rotaResult: %d", pb_out->fileType, pb_out->rotaResult);
    return 0;
}

int pb_packet_rotas_file_status_rsp(rotas_file_status_rsp_t *in, RotasFileStatusRsp *pb_out)
{
    pb_out->fileType = (RotasUpdateFile)in->fileType;
    pb_out->packetSize = in->packetSize;
    pb_out->result = (RotaErrorCode)in->result;

    LOG_PB("fileType: %d, result: %d", pb_out->fileType, pb_out->result);
    return 0;
}


int pb_packet_ota_update_info(ota_update_info_t *in, OtaUpdateInfo *pb_out)
{
    pb_out->model_id = in->model_id;
    pb_out->version_type = in->version_type;
    pb_out->version_code = in->version_code;
    pb_out->hardware_id = in->hardware_id;

    XJYD_STRNCPY(pb_out->device_id, in->device_id, sizeof(pb_out->device_id)); 

    return 0;
}

int pb_packet_file_info(iot_file_info_t *in, FileInfo *pb_out)
{
    pb_out->file_id = in->file_id;
    
    return 0;
}


