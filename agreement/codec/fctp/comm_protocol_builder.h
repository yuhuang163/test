#ifndef __COMM_PROTOCOL_BUILDER_H__
#define __COMM_PROTOCOL_BUILDER_H__

#ifdef __cplusplus
extern "C" {
#endif

/*******************************************************************************
* Include files
******************************************************************************/
#include "stdint.h" // IWYU pragma: keep
#include "comm_protocol_defs.h"

/*
* INTERNAL FUNCTION DECLARATION
*****************************************************************************************
*/
int comm_protocol_builder_init(comm_protocol_builder_t *builder, uint16_t max_size, uint16_t seq, uint8_t frame_type);

comm_protocol_service_t *comm_protocol_builder_init_service(comm_protocol_builder_t *builder, uint16_t service_id);

int comm_protocol_builder_add_tlv(comm_protocol_builder_t *builder, comm_protocol_service_t *service,
                    comm_protocol_tlv_node_t *tlv_node);

uint8_t* comm_protocol_builder_finalize(comm_protocol_builder_t *builder, uint16_t *out_size);

void comm_protocol_builder_free(comm_protocol_builder_t *builder);

#ifdef __cplusplus
}
#endif

#endif 