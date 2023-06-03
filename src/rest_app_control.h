#ifndef REST_APP_CONTROL_H_
#define REST_APP_CONTROL_H_

#include <stdbool.h>

// The REST App control is designed to replace the button event while the OpenTrickler is operated under screenless mode. 
// On REST action is issued from the REST endpoint, the REST control will send OVERRIDE_FROM_REST to the button queue to
// request the application to poll from the REST action queue, then act accoridngly.

typedef enum {
    REST_CTRL_NEXT,
    REST_CTRL_PREV,
} rest_control_event_t;


#ifdef __cplusplus
extern "C" {
#endif


void rest_app_control_init(void);
rest_control_event_t rest_app_control_wait_for_event(bool block);

#ifdef __cplusplus
}
#endif


#endif  // REST_APP_CONTROL_H_