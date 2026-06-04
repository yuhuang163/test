#include "usmile_app_internal.h" 
#include "pb_common_part_codec.h"
#include "ble_protocol_process.h"
#include "nanopb_tools.h"

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
    pb_out->sku_id           = in->sku_id;
    pb_out->sn_id.size = strlen(in->sn);
    memcpy(pb_out->sn_id.bytes, in->sn, sizeof(pb_out->sn_id.bytes));
    
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

int pb_packet_upload_brush_report(void *context, brush_report_info_t *in, BrushingReport *pb_out)
{
    int real_count = 0;
 
    real_count = MIN(in->count, 5);
    LOG_PB("count: %d, real_count: %d", in->count, real_count);

    for (int i = 0; i < real_count; i++) {
        pb_out->report_count++;
        pb_out->report_data_count++;
        pb_packet_get_brush_report(context, &pb_out->report_data[i], &in->info[i]);
    }

    pb_out->sync_result = ErrorCode_SUCCESS;

    return 0;
}

int pb_packet_upload_sensor_data(data_burial_point_t *in, SensorData *pb_out)
{
    static struct pb_bytes_array byteData;
    pb_out->timestamp = in->timestamp;
    pb_out->packet_total = in->packet_total;
    pb_out->packet_index = in->packet_index;

    XJYD_STRNCPY(pb_out->md5, in->md5, USMILE_MD5_STRING_LEN);
    pb_out->data_block.funcs.encode = nanopb_encode_map_bytes;
    pb_out->data_block.arg = &byteData;
    byteData.bytes = in->data_block;
    byteData.size = in->data_block_size;

    return 0;
}


int pb_packet_audio_info(audio_data_info_t *in, AudioDataInfo *pb_out)
{
    pb_out->file_id = in->file_id;
    pb_out->speech_id = in->speech_id;
    pb_out->music_conductor_switch = in->music_conductor_switch;
    pb_out->use_remind_type = in->use_remind_type;
    pb_out->setting_result = in->setting_result;
    pb_out->timestamp = in->timestamp;
    return 0;
}


#if LED_LIGHTING_CUSTOM
int pb_packet_light_custom_info(light_custom_info_t *in, LightCustomInfo *pb_out)
{
    pb_out->file_id = in->file_id;
    pb_out->light_id = in->light_id;
    pb_out->light_type = in->light_type;
    pb_out->start_time = in->start_time;
    pb_out->cycle_time = in->cycle_time;
    pb_out->setting_result = in->setting_result;
    pb_out->light_loop_switch = in->light_loop_switch;
    return 0;
}
#endif

int pb_packet_device_state(device_state_t *in, DeviceState *pb_out)
{
    pb_out->state_value_count = in->state_value_count;
    for(int i=0; i<in->state_value_count; i++)
    {
        switch(in->state_value[i].item)
        {
            case DEVITEM_MUSIC_MODEL_SWITCH:
                pb_out->state_value[i].item = DeviceItem_MUSIC_MODEL_SWITCH;
                pb_out->state_value[i].which_value_item = DeviceValue_music_model_switch_tag;
                pb_out->state_value[i].value_item.music_model_switch = in->state_value[i].value_item.music_model_switch;
                break;
            case DEVITEM_MUSIC_ENJOY_MODEL_SWITCH:
                pb_out->state_value[i].item = DeviceItem_MUSIC_ENJOY_MODEL_SWITCH;
                pb_out->state_value[i].which_value_item = DeviceValue_music_enjoy_model_switch_tag;
                pb_out->state_value[i].value_item.music_enjoy_model_switch = in->state_value[i].value_item.music_enjoy_model_switch;
                break;
            case DEVITEM_VOICE_GUIDANCE_TYPE:
                pb_out->state_value[i].item = DeviceItem_VOICE_GUIDANCE_TYPE;
                pb_out->state_value[i].which_value_item = DeviceValue_voice_guidance_type_tag;
                pb_out->state_value[i].value_item.voice_guidance_type = in->state_value[i].value_item.voice_guidance_type;
                break;                
            default: break;
        }        
    }
    
    pb_out->setting_result = in->result;
    
    return 0;
}

int pb_packet_rotas_result_req(rotas_result_req_t *in, RotasResultReq *pb_out)
{
    pb_out->fileType = (RotasUpdateFile)in->fileType;
    pb_out->fileVersion = in->fileVersion;
    pb_out->rotaResult = (RotaErrorCode)in->rotaResult;
    pb_out->sub_error_code = in->sub_error_code;
    
    LOG_PB("fileType: %d, rotaResult: %d, sub_error_code: 0x%lx", pb_out->fileType, pb_out->rotaResult, in->sub_error_code);
    return 0;
}

int pb_packet_rotas_result_rsp(rotas_data_rsp_t *in, RotasResultRsp *pb_out) 
{
    pb_out->fileType = in->fileType;
    pb_out->result = in->result;
    pb_out->rotaStatus = in->rotaStatus;
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


int pb_packet_display_setting_info(display_setting_info_t *in, DisplaySettingInfo *pb_out)
{
    pb_out->display_content_count = in->display_content_count;

    for(int i=0; i<pb_out->display_content_count; i++) {
        pb_out->display_content[i].display_state = in->display_content[i].display_state;
        pb_out->display_content[i].display_style = in->display_content[i].display_style;
    }

    pb_out->setting_result = in->setting_result;
    
    return 0;
}

int pb_packet_audio_remind_state(audio_remind_data_t *in, AudioRemindData *pb_out)
{
    pb_out->setting_result = (ErrorCode)in->setting_result;
    pb_out->remind_state_count = in->count;
    for (uint8_t i = 0; i < in->count; i++) {
        pb_out->remind_state[i].remind_type = (RemindType)in->state[i].remind_type;
        pb_out->remind_state[i].remind_switch = (SwitchState)in->state[i].remind_switch;
        pb_out->remind_state[i].select_item = in->state[i].select_item;
        pb_out->remind_state[i].which_state_value = AudioRemindState_type_value_tag;
        pb_out->remind_state[i].state_value.type_value = in->state[i].remind_type;
        LOG_PB("type_value: %ld", pb_out->remind_state[i].state_value.type_value);
    }

    return 0;
}

