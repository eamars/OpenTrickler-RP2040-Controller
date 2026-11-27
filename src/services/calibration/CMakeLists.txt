ot_module(SRCS calibration.c
          INCLUDE_DIRS include
          REQUIRES activity calibration_method
          PRIV_REQUIRES charge events freertos jobs motors platform profile scale settings)
