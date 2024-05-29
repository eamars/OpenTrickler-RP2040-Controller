#ifndef APP_HEADER_H_
#define APP_HEADER_H_

#include <stdint.h>

typedef struct {
    uint32_t app_header_rev;
    uint32_t flash_partition_idx;
    uint32_t crc;
    char vcs_hash[16];
    char build_type[16];
    char firmware_version[32];

} app_header_t;

#ifdef __cplusplus
extern "C"
{
#endif

const app_header_t * get_app_header();

#ifdef __cplusplus
}
#endif


#endif  // APP_HEADER_H_