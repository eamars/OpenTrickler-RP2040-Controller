#ifndef SCALE_H_
#define SCALE_H_



// Interface functions
#ifdef __cplusplus
extern "C" {
#endif


bool scale_init();
void scale_task(void *p);
float scale_get_current_measurement();
float scale_block_wait_for_next_measurement();

#ifdef __cplusplus
}
#endif


#endif