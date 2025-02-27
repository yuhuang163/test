
#ifndef __MD5_H__
#define __MD5_H__
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

#define MD5_DIGEST_LENGTH (16)
#define MD5_GEN(src, len) md5_gen((const uint8_t *)(src), (len))

typedef struct {
    unsigned int state[4];     
    unsigned int count[2];     
    unsigned char buffer[64];     
} MD5Context;
 
void MD5_Init(MD5Context * context);
void MD5_Update(MD5Context * context, unsigned char * buf, int len);
void MD5_Final(MD5Context * context, unsigned char *digest);

const uint8_t *md5_gen(const uint8_t *src, uint32_t src_len);

void *init_md5(void);
void update_md5(void *ctx, void *src, int len);
uint8_t *final_md5(void *ctx);

#endif
