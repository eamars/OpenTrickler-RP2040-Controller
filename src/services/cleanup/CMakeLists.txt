ot_module(SRCS cleanup_mode.c
          INCLUDE_DIRS include
          REQUIRES activity motors
          PRIV_REQUIRES charge events freertos jobs mini_12864 scale servo_gate storage uc1701)
