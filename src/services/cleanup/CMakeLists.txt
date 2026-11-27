ot_module(SRCS cleanup_mode.cpp
          INCLUDE_DIRS include
          REQUIRES activity motors
          PRIV_REQUIRES charge events freertos jobs menu mini_12864 scale servo_gate storage uc1701)
