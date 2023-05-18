#ifndef REST_ENDPOINTS_H_
#define REST_ENDPOINTS_H_

#include <stdbool.h>


typedef enum {
    HTTP_GET,
    HTTP_POST,
} http_method_t;

typedef bool *(rest_handler_t)(char *, char *); 

#ifdef __cplusplus
extern "C" {
#endif


void rest_register_handler(char * request, rest_handler_t f);
rest_handler_t * rest_get_handler(char *request);
bool rest_endpoint_init();

#ifdef __cplusplus
}  // __cplusplus
#endif


#endif  // REST_ENDPOINTS_H_
