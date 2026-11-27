ot_module(SRCS cleanup_mode.c
          INCLUDE_DIRS include
          REQUIRES activity motors
          PRIV_REQUIRES charge events freertos jobs scale servo_gate storage)
