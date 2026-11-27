ot_module(SRCS motors.c
          INCLUDE_DIRS include
          REQUIRES freertos platform storage
          PRIV_REQUIRES board events rp2_sdk settings tmc2209)
