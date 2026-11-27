/*
 * FreeRTOS's port on the RP chips (rtos_port.h): a stack counted in words, a critical section with no argument.
 */
#include "rtos_port.h"


bool rtos_task_start(TaskFunction_t run, const char *name, uint32_t stack_bytes, void *arg, UBaseType_t priority,
                     TaskHandle_t *handle) {
    configASSERT(stack_bytes % sizeof(StackType_t) == 0);
    return xTaskCreate(run, name, (configSTACK_DEPTH_TYPE) (stack_bytes / sizeof(StackType_t)), arg, priority,
                       handle) == pdPASS;
}


void rtos_critical_enter(void) {
    taskENTER_CRITICAL();
}


void rtos_critical_exit(void) {
    taskEXIT_CRITICAL();
}


UBaseType_t rtos_critical_enter_from_isr(void) {
    return taskENTER_CRITICAL_FROM_ISR();
}


void rtos_critical_exit_from_isr(UBaseType_t saved) {
    taskEXIT_CRITICAL_FROM_ISR(saved);
}
