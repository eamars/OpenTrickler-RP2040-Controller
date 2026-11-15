#ifndef API_V1_H_
#define API_V1_H_
/*
 * The routes of API v1 (ADR-003; docs/developments_plans/ADR-003-http_rest_server/api_v1.md). Gate 2 serves the
 * first of them: the charge's state, the screen, and /api/v1/debug's figures and its two routes for the proofs.
 */
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool api_v1_init(void);

#ifdef __cplusplus
}
#endif

#endif  // API_V1_H_
