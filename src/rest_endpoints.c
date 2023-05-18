#include <string.h>
#include <stdlib.h>
#include "rest_endpoints.h"



typedef struct _rest_endpoint_node{
    char * request;
    rest_handler_t * function_handler;
    struct _rest_endpoint_node * next;
} _rest_endpoint_node_t;


// Locals
static _rest_endpoint_node_t * rest_endpoint_head = NULL;


void rest_register_handler(char * request, rest_handler_t f) {
    _rest_endpoint_node_t * new_node = malloc(sizeof(_rest_endpoint_node_t));
    new_node->request = strdup(request);

    // Add to the head
    new_node->next = rest_endpoint_head;
    rest_endpoint_head = new_node;
}


rest_handler_t * rest_get_handler(char * request) {
    rest_handler_t * target_handler = NULL;
    for (_rest_endpoint_node_t * node = rest_endpoint_head; node->next != NULL; node = node->next) {
        if (strcmp(request, node->request) == 0) {
            target_handler = node->function_handler;
            break;
        }
    }

    return target_handler;
}


bool eeprom_endpoint(char * request, char * param){

}


bool rest_endpoint_init() {
    // Add charge mode endpoint
    // rest_register_handler("/eeprom")
}