ot_module(SRCS motors.c
          INCLUDE_DIRS include
          REQUIRES freertos platform storage
          PRIV_REQUIRES board events settings tmc2209)
