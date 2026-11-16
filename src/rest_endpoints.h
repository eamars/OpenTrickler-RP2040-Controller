#ifndef REST_ENDPOINTS_H_
#define REST_ENDPOINTS_H_

#include <stdbool.h>



#ifdef __cplusplus
extern "C" {
#endif


bool rest_endpoints_init(bool default_wizard);
// The root page: the wizard as the unit's own access point, the portal once it is on its network (ADR-004)
void rest_endpoints_set_root_wizard(bool wizard);




#ifdef __cplusplus
}  // __cplusplus
#endif


#endif  // REST_ENDPOINTS_H_
