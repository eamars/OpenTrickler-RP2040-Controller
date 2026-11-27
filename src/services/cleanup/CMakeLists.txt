ot_module(SRCS cleanup_mode.c
          INCLUDE_DIRS include
          REQUIRES activity motors
          PRIV_REQUIRES charge events freertos gate jobs scale storage)
