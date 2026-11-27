// The makes' host: the scale's service, handed over before any make's task runs (scale_make.h)
#include "scale_make.h"

const scale_make_host_t *scale_make_host = NULL;


void scale_makes_set_host(const scale_make_host_t *host) {
    scale_make_host = host;
}
