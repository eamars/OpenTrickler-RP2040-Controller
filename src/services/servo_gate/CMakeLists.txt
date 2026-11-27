ot_module(SRCS servo_gate.c
          INCLUDE_DIRS include
          REQUIRES freertos gate
          PRIV_REQUIRES board platform settings storage)
