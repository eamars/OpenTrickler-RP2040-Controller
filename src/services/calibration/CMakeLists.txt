ot_module(SRCS calibration.c
          INCLUDE_DIRS include
          REQUIRES activity calibration_method
          PRIV_REQUIRES charge events freertos jobs menu motors profile rp2_sdk scale settings)
