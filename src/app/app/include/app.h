#ifndef APP_H_
#define APP_H_

#include "eeprom.h"


typedef struct {
    
} app_persistent_config_t;


typedef struct {
    app_persistent_config_t persistent_config;
} app_config_t;


#ifdef __cplusplus
extern "C" {
#endif

bool app_init();
bool http_app_config();

#ifdef __cplusplus
}
#endif

#endif  // APP_H_
