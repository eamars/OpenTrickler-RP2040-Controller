#include "app_header.h"

#ifdef __cplusplus
    extern "C"
    {
#endif

__attribute__ ((section (".app_header")))
__attribute__ ((used))
const app_header_t __app_header = {
    .app_header_rev = 1,
    .flash_partition_idx = 0,
    .vcs_hash = "0x12345",
    .build_type = "Debug",
    .firmware_version = "1.2.3-dirty",
};


#ifdef __cplusplus
    }
#endif