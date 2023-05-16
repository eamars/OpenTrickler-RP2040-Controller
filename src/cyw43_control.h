#ifndef CYW43_CONTROL_H_
#define CYW43_CONTROL_H_

#ifdef __cplusplus
extern "C" {
#endif

void cyw43_start(bool block);
void cyw43_stop(bool block);
void cyw43_task(void*);


#ifdef __cplusplus
}  // __cplusplus
#endif


#endif  // CYW43_CONTROL_H_